"""Password hashing, JWT creation/verification, and auth dependencies."""
from datetime import datetime, timedelta, timezone
from typing import Annotated

from fastapi import Depends, Header, HTTPException, status
from fastapi.security import OAuth2PasswordBearer
import bcrypt
from jose import JWTError, jwt
from sqlalchemy.orm import Session

from .config import get_settings
from .database import get_db
from .models import Device, Role, Tier, User

settings = get_settings()
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/auth/login", auto_error=False)


def hash_password(password: str) -> str:
    # bcrypt only considers the first 72 bytes; truncate explicitly to avoid errors.
    return bcrypt.hashpw(password.encode("utf-8")[:72], bcrypt.gensalt()).decode("utf-8")


def verify_password(plain: str, hashed: str) -> bool:
    return bcrypt.checkpw(plain.encode("utf-8")[:72], hashed.encode("utf-8"))


def _create_token(claims: dict, expires: timedelta) -> str:
    payload = claims.copy()
    payload["exp"] = datetime.now(timezone.utc) + expires
    return jwt.encode(payload, settings.jwt_secret, algorithm=settings.jwt_algorithm)


def create_access_token(user: User, impersonated_by: int | None = None) -> str:
    claims = {"sub": str(user.id), "email": user.email, "role": user.role.value,
              "tier": user.tier.value, "type": "access"}
    if impersonated_by is not None:
        claims["impersonatedBy"] = impersonated_by
        ttl = timedelta(minutes=settings.impersonation_ttl_minutes)
    else:
        ttl = timedelta(minutes=settings.access_token_ttl_minutes)
    return _create_token(claims, ttl)


def create_refresh_token(user: User) -> str:
    return _create_token({"sub": str(user.id), "type": "refresh"},
                         timedelta(days=settings.refresh_token_ttl_days))


def decode_token(token: str) -> dict:
    try:
        return jwt.decode(token, settings.jwt_secret, algorithms=[settings.jwt_algorithm])
    except JWTError:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Invalid or expired token")


def get_current_user(token: Annotated[str | None, Depends(oauth2_scheme)],
                     db: Annotated[Session, Depends(get_db)]) -> User:
    if not token:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Not authenticated")
    payload = decode_token(token)
    if payload.get("type") != "access":
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Not an access token")
    user = db.get(User, int(payload.get("sub", 0)))
    if not user or not user.is_active:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "User not found or inactive")
    return user


def require_super_admin(user: Annotated[User, Depends(get_current_user)]) -> User:
    if user.role != Role.SUPER_ADMIN:
        raise HTTPException(status.HTTP_403_FORBIDDEN, "Super admin required")
    return user


def require_premium(user: Annotated[User, Depends(get_current_user)]) -> User:
    if user.tier != Tier.PREMIUM and user.role != Role.SUPER_ADMIN:
        raise HTTPException(status.HTTP_402_PAYMENT_REQUIRED, "Premium feature")
    return user


def get_device_by_api_key(db: Annotated[Session, Depends(get_db)],
                          x_api_key: Annotated[str | None, Header()] = None) -> Device:
    if not x_api_key:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Missing X-API-Key")
    device = db.query(Device).filter(Device.api_key == x_api_key).first()
    if not device:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Invalid device API key")
    return device
