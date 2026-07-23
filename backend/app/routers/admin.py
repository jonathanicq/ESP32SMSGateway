"""Super-admin impersonation (non-negotiable #20)."""
import logging
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import User
from ..schemas import TokenPair
from ..security import create_access_token, require_super_admin

router = APIRouter(prefix="/admin", tags=["admin"])
audit = logging.getLogger("impersonation-audit")


@router.post("/impersonate/{user_id}", response_model=TokenPair)
def impersonate(user_id: int, admin: Annotated[User, Depends(require_super_admin)],
                db: Annotated[Session, Depends(get_db)]) -> TokenPair:
    target = db.get(User, user_id)
    if not target:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "User not found")
    if target.role.value == "SUPER_ADMIN":
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "Cannot impersonate another super admin")

    # Audit every impersonation grant. No chaining: token carries the original admin id.
    audit.warning("IMPERSONATION admin_id=%s -> user_id=%s (%s)",
                  admin.id, target.id, target.email)
    token = create_access_token(target, impersonated_by=admin.id)
    return TokenPair(access_token=token, refresh_token="")  # no refresh while impersonating
