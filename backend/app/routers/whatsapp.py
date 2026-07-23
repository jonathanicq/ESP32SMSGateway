"""WhatsApp bridge (premium): per-user config + WAHA inbound webhook."""
import logging
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy.orm import Session

from ..config import get_settings
from ..database import get_db
from ..models import BridgeSettings, Device, Direction, Message, MessageStatus, Tier, User
from ..schemas import BridgeSettingsIn, BridgeSettingsOut
from ..security import get_current_user, require_premium
from ..whatsapp import normalize_chat_to_number, parse_keyword_command, send_whatsapp

logger = logging.getLogger("whatsapp-router")
settings = get_settings()
router = APIRouter(prefix="/whatsapp", tags=["whatsapp"])


# --- Bridge config (premium-gated) ---
@router.get("/bridge", response_model=BridgeSettingsOut)
def get_bridge(user: Annotated[User, Depends(require_premium)],
               db: Annotated[Session, Depends(get_db)]) -> BridgeSettings:
    bridge = db.query(BridgeSettings).filter(BridgeSettings.owner_id == user.id).first()
    if not bridge:
        bridge = BridgeSettings(owner_id=user.id)
        db.add(bridge)
        db.commit()
        db.refresh(bridge)
    return bridge


@router.put("/bridge", response_model=BridgeSettingsOut)
def update_bridge(body: BridgeSettingsIn, user: Annotated[User, Depends(require_premium)],
                  db: Annotated[Session, Depends(get_db)]) -> BridgeSettings:
    if body.send_device_id is not None:
        dev = db.get(Device, body.send_device_id)
        if not dev or dev.owner_id != user.id:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "send_device_id is not your device")
    bridge = db.query(BridgeSettings).filter(BridgeSettings.owner_id == user.id).first()
    if not bridge:
        bridge = BridgeSettings(owner_id=user.id)
        db.add(bridge)
    bridge.enabled = body.enabled
    bridge.waha_session = body.waha_session
    bridge.forward_to_chat = body.forward_to_chat
    bridge.send_device_id = body.send_device_id
    db.commit()
    db.refresh(bridge)
    return bridge


# --- WAHA inbound webhook (no JWT; protected by a shared token) ---
@router.post("/webhook")
def waha_webhook(payload: dict, db: Annotated[Session, Depends(get_db)],
                 token: str = Query(default="")) -> dict:
    if token != settings.whatsapp_webhook_token:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Bad webhook token")

    # WAHA "message" event shape: {event, session, payload: {from, body, fromMe, ...}}
    if payload.get("event") not in (None, "message"):
        return {"ignored": payload.get("event")}
    msg = payload.get("payload", {})
    if msg.get("fromMe"):
        return {"ignored": "fromMe"}
    session = payload.get("session", settings.waha_session)
    chat_from = msg.get("from", "")
    text = msg.get("body", "") or ""

    # Resolve the owner bridge for this WAHA session.
    bridge = db.query(BridgeSettings).filter(
        BridgeSettings.waha_session == session, BridgeSettings.enabled.is_(True)
    ).first()
    if not bridge:
        return {"ignored": "no enabled bridge for session"}

    owner = db.get(User, bridge.owner_id)
    if not owner or (owner.tier != Tier.PREMIUM and owner.role.value != "SUPER_ADMIN"):
        return {"ignored": "owner not premium"}

    command = parse_keyword_command(text)
    if not command:
        return {"ignored": "no trigger match"}
    recipient, body = command

    if not bridge.send_device_id:
        send_whatsapp(chat_from, "⚠️ Bridge has no send device configured.", session)
        return {"error": "no send_device_id"}
    device = db.get(Device, bridge.send_device_id)
    if not device or device.owner_id != owner.id:
        return {"error": "invalid send device"}

    # Enqueue the transcoded outbound SMS — the Android gateway will pick it up.
    sms = Message(device_id=device.id, direction=Direction.OUTBOUND,
                  phone_number=recipient, text=body, status=MessageStatus.PENDING)
    db.add(sms)
    db.commit()
    db.refresh(sms)
    send_whatsapp(chat_from, f"✅ SMS queued to {recipient} (#{sms.id})", session)
    logger.info("WhatsApp→SMS: %s queued SMS #%s to %s", normalize_chat_to_number(chat_from), sms.id, recipient)
    return {"queued_sms_id": sms.id, "to": recipient}


# --- Status (any authenticated user) ---
@router.get("/status")
def bridge_status(user: Annotated[User, Depends(get_current_user)]) -> dict:
    from ..whatsapp import waha_configured
    return {
        "waha_configured": waha_configured(),
        "trigger_keyword": settings.whatsapp_trigger_keyword,
        "premium": user.tier == Tier.PREMIUM or user.role.value == "SUPER_ADMIN",
    }
