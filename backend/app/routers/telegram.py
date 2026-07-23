"""Telegram webhook and bridge endpoints."""
import logging
from datetime import datetime, timezone

from fastapi import APIRouter, Depends, HTTPException, Request, status
from sqlalchemy.orm import Session

from ..config import get_settings
from ..database import SessionLocal
from ..models import BridgeSettings, Device, Message, MessageStatus, User
from ..security import get_current_user
from ..telegram import (
    format_sms_to_telegram_html,
    parse_keyword_command,
    send_telegram,
    telegram_configured,
)

logger = logging.getLogger("telegram_routes")
settings = get_settings()

router = APIRouter(prefix="/telegram", tags=["telegram"])


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


@router.get("/settings")
async def get_telegram_settings(
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    """Get user's Telegram bridge settings."""
    settings_row = db.query(BridgeSettings).filter_by(owner_id=current_user.id).first()
    if not settings_row:
        return {
            "enabled": False,
            "telegram_enabled": False,
            "telegram_bot_token": None,
            "telegram_chat_id": None,
        }
    return {
        "enabled": settings_row.enabled,
        "telegram_enabled": settings_row.telegram_enabled,
        "telegram_bot_token": settings_row.telegram_bot_token or "",
        "telegram_chat_id": settings_row.telegram_chat_id or "",
        "send_device_id": settings_row.send_device_id,
    }


@router.post("/settings")
async def update_telegram_settings(
    data: dict,
    current_user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    """Update user's Telegram bridge settings."""
    settings_row = db.query(BridgeSettings).filter_by(owner_id=current_user.id).first()
    if not settings_row:
        settings_row = BridgeSettings(owner_id=current_user.id)
        db.add(settings_row)

    settings_row.telegram_enabled = data.get("telegram_enabled", False)
    settings_row.telegram_bot_token = data.get("telegram_bot_token", "").strip() or None
    settings_row.telegram_chat_id = data.get("telegram_chat_id", "").strip() or None

    if data.get("send_device_id"):
        device = db.query(Device).filter_by(
            id=data["send_device_id"], owner_id=current_user.id
        ).first()
        if not device:
            raise HTTPException(status_code=404, detail="Device not found")
        settings_row.send_device_id = device.id

    db.commit()
    return {"status": "ok"}


@router.post("/webhook")
async def telegram_webhook(request: Request, db: Session = Depends(get_db)):
    """Telegram webhook: receive messages and transcode to SMS."""
    try:
        payload = await request.json()
    except Exception:
        return {"ok": True}

    message = payload.get("message")
    if not message:
        return {"ok": True}

    chat_id = str(message.get("chat", {}).get("id"))
    text = message.get("text", "").strip()
    sender_id = str(message.get("from", {}).get("id"))
    sender_name = message.get("from", {}).get("first_name", "User")

    if not text:
        return {"ok": True}

    # Find the user by Telegram chat ID
    bridge_settings = (
        db.query(BridgeSettings)
        .filter_by(telegram_enabled=True, telegram_chat_id=chat_id)
        .first()
    )

    if not bridge_settings:
        logger.debug("No bridge found for Telegram chat %s", chat_id)
        return {"ok": True}

    # Parse SMS command (e.g., "/sms +1234567890 Hello world")
    cmd_result = parse_keyword_command(text, trigger=settings.telegram_sms_trigger_keyword)
    if not cmd_result:
        # Not an SMS command, just log it
        logger.debug("Message from %s is not an SMS command: %s", sender_name, text)
        return {"ok": True}

    recipient, body = cmd_result
    device = db.query(Device).filter_by(id=bridge_settings.send_device_id).first()
    if not device:
        logger.warning("Bridge device not found for user %d", bridge_settings.owner_id)
        return {"ok": True}

    # Create outbound message (the device will actually send it via SMS)
    msg = Message(
        device_id=device.id,
        direction="outbound",
        phone_number=recipient,
        text=body,
        status=MessageStatus.PENDING,
    )
    db.add(msg)
    db.commit()

    logger.info(
        "Created SMS message %d from Telegram: %s → %s", msg.id, sender_name, recipient
    )
    return {"ok": True}


def telegram_forward_sms(sender: str, body: str, device_name: str = "", token: str | None = None, chat_id: str | None = None) -> bool:
    """Forward an inbound SMS to Telegram."""
    msg = format_sms_to_telegram_html(sender, body, device_name)
    return send_telegram(msg, token, chat_id)
