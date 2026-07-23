"""Device registration + management (user-facing)."""
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from datetime import datetime, timezone

from ..database import get_db
from ..models import Device, PairingRequest, User
from ..schemas import ClaimRequest, DeviceCreate, DeviceOut
from ..security import get_current_user

router = APIRouter(prefix="/devices", tags=["devices"])


@router.post("", response_model=DeviceOut, status_code=status.HTTP_201_CREATED)
def create_device(body: DeviceCreate, user: Annotated[User, Depends(get_current_user)],
                  db: Annotated[Session, Depends(get_db)]) -> Device:
    device = Device(name=body.name, owner_id=user.id)
    db.add(device)
    db.commit()
    db.refresh(device)
    return device


@router.get("", response_model=list[DeviceOut])
def list_devices(user: Annotated[User, Depends(get_current_user)],
                 db: Annotated[Session, Depends(get_db)]) -> list[Device]:
    return db.query(Device).filter(Device.owner_id == user.id).all()


@router.post("/claim", response_model=DeviceOut, status_code=status.HTTP_201_CREATED)
def claim_device(body: ClaimRequest, user: Annotated[User, Depends(get_current_user)],
                 db: Annotated[Session, Depends(get_db)]) -> Device:
    """Link a pending pairing (by claim code shown on the phone) to the current user."""
    code = body.claim_code.strip().upper()
    pr = db.query(PairingRequest).filter(
        PairingRequest.claim_code == code, PairingRequest.status == "pending"
    ).first()
    if not pr:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Invalid or already-used code")
    expires_at = pr.expires_at if pr.expires_at.tzinfo else pr.expires_at.replace(tzinfo=timezone.utc)
    if datetime.now(timezone.utc) > expires_at:
        raise HTTPException(status.HTTP_410_GONE, "Code expired — generate a new one on the device")

    device = Device(name=body.name or pr.device_name, owner_id=user.id)
    db.add(device)
    db.flush()  # populate device.api_key
    pr.status = "linked"
    pr.api_key = device.api_key
    db.commit()
    db.refresh(device)
    return device


@router.delete("/{device_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_device(device_id: int, user: Annotated[User, Depends(get_current_user)],
                  db: Annotated[Session, Depends(get_db)]) -> None:
    device = db.get(Device, device_id)
    if not device or device.owner_id != user.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Device not found")
    db.delete(device)
    db.commit()
