"""Health endpoint (non-negotiable #9) — no auth."""
from datetime import datetime, timezone
from typing import Annotated

from fastapi import APIRouter, Depends
from sqlalchemy import text
from sqlalchemy.orm import Session

from ..config import get_settings
from ..database import get_db

router = APIRouter(tags=["health"])
settings = get_settings()


@router.get("/health")
def health(db: Annotated[Session, Depends(get_db)]) -> dict:
    db_ok = True
    try:
        db.execute(text("SELECT 1"))
    except Exception:  # noqa: BLE001
        db_ok = False

    return {
        "status": "ok" if db_ok else "degraded",
        "version": settings.app_version,
        "commit": settings.git_commit,
        "checks": {"database": "ok" if db_ok else "error"},
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }
