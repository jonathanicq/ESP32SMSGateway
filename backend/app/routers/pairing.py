"""Device pairing (public): app starts a pairing, user claims the code in the backoffice."""
import secrets
from datetime import datetime, timedelta, timezone
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import PairingRequest
from ..schemas import PairPollRequest, PairPollResponse, PairStartRequest, PairStartResponse

router = APIRouter(prefix="/pair", tags=["pairing"])

# Unambiguous alphabet (no 0/O/1/I) for human-entered claim codes.
_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
PAIRING_TTL_MINUTES = 15


def _gen_code(n: int = 6) -> str:
    return "".join(secrets.choice(_ALPHABET) for _ in range(n))


def _utcnow() -> datetime:
    return datetime.now(timezone.utc)


@router.post("/start", response_model=PairStartResponse)
def pair_start(body: PairStartRequest, db: Annotated[Session, Depends(get_db)]) -> PairStartResponse:
    # Ensure the claim code is unique among active requests.
    code = _gen_code()
    while db.query(PairingRequest).filter(PairingRequest.claim_code == code).first():
        code = _gen_code()
    pr = PairingRequest(
        claim_code=code,
        poll_token=secrets.token_urlsafe(32),
        device_name=body.device_name,
        status="pending",
        expires_at=_utcnow() + timedelta(minutes=PAIRING_TTL_MINUTES),
    )
    db.add(pr)
    db.commit()
    db.refresh(pr)
    return PairStartResponse(claim_code=pr.claim_code, poll_token=pr.poll_token,
                             expires_in=PAIRING_TTL_MINUTES * 60)


@router.post("/poll", response_model=PairPollResponse)
def pair_poll(body: PairPollRequest, db: Annotated[Session, Depends(get_db)]) -> PairPollResponse:
    pr = db.query(PairingRequest).filter(PairingRequest.poll_token == body.poll_token).first()
    if not pr:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Unknown pairing")
    if pr.status == "linked":
        return PairPollResponse(status="linked", api_key=pr.api_key)
    expires_at = pr.expires_at
    if expires_at.tzinfo is None:
        expires_at = expires_at.replace(tzinfo=timezone.utc)
    if _utcnow() > expires_at:
        return PairPollResponse(status="expired")
    return PairPollResponse(status="pending")
