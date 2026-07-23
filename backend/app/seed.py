"""Seed the mandatory master account on first startup (non-negotiable #19)."""
import logging

from sqlalchemy.orm import Session

from .config import get_settings
from .models import Role, Tier, User
from .security import hash_password

logger = logging.getLogger("seed")
settings = get_settings()


def seed_super_admin(db: Session) -> None:
    existing = db.query(User).filter(User.role == Role.SUPER_ADMIN).first()
    if existing:
        return  # never overwrite an existing super admin

    admin = User(
        email=settings.super_admin_email,
        hashed_password=hash_password(settings.super_admin_password),
        role=Role.SUPER_ADMIN,
        tier=Tier.PREMIUM,
        is_active=True,
    )
    db.add(admin)
    db.commit()
    logger.info("Seeded SUPER_ADMIN account: %s", settings.super_admin_email)

    if settings.is_production and settings.super_admin_password == "ChangeMe123!":
        logger.warning("SECURITY: default SUPER_ADMIN password is still in use in PRODUCTION!")
