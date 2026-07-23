"""Message endpoints: user-facing (send/query) + device-facing (poll/status/inbound)."""
import logging
from datetime import datetime, timezone
from typing import Annotated

import httpx
from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import (BridgeSettings, Device, Direction, Message, MessageStatus,
                      Tier, User, Webhook)
from ..schemas import (InboundMessageRequest, MessageOut, SendMessageRequest,
                       StatusUpdateRequest)
from ..security import get_current_user, get_device_by_api_key
from ..telegram import telegram_forward_sms
from ..whatsapp import send_whatsapp

logger = logging.getLogger("messages")
router = APIRouter(tags=["messages"])

_STATUS_EVENT = {
    MessageStatus.SENT: "sms:sent",
    MessageStatus.DELIVERED: "sms:delivered",
    MessageStatus.FAILED: "sms:failed",
    MessageStatus.RECEIVED: "sms:received",
}


def _dispatch_webhooks(db: Session, owner_id: int, event: str, message: Message) -> None:
    """Fire matching webhooks best-effort. Never raises."""
    hooks = db.query(Webhook).filter(
        Webhook.owner_id == owner_id, Webhook.event == event, Webhook.is_active.is_(True)
    ).all()
    if not hooks:
        return
    payload = {
        "event": event,
        "message": {
            "id": message.id, "device_id": message.device_id,
            "direction": message.direction.value, "phone_number": message.phone_number,
            "text": message.text, "status": message.status.value,
        },
    }
    for hook in hooks:
        try:
            httpx.post(hook.url, json=payload, timeout=5)
        except Exception as exc:  # noqa: BLE001 — webhook delivery is best-effort
            logger.warning("webhook %s -> %s failed: %s", hook.id, hook.url, exc)


# --- User-facing ---
@router.post("/messages", response_model=MessageOut, status_code=status.HTTP_201_CREATED)
def send_message(body: SendMessageRequest, user: Annotated[User, Depends(get_current_user)],
                 db: Annotated[Session, Depends(get_db)]) -> Message:
    device = db.get(Device, body.device_id)
    if not device or device.owner_id != user.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Device not found")
    msg = Message(device_id=device.id, direction=Direction.OUTBOUND,
                  phone_number=body.phone_number, text=body.text,
                  status=MessageStatus.PENDING)
    db.add(msg)
    db.commit()
    db.refresh(msg)
    return msg


@router.get("/messages", response_model=list[MessageOut])
def list_messages(user: Annotated[User, Depends(get_current_user)],
                  db: Annotated[Session, Depends(get_db)],
                  device_id: int | None = None,
                  direction: Direction | None = None,
                  msg_status: MessageStatus | None = Query(default=None, alias="status"),
                  limit: int = Query(default=50, le=500)) -> list[Message]:
    owned = db.query(Device.id).filter(Device.owner_id == user.id).subquery()
    q = db.query(Message).filter(Message.device_id.in_(owned))
    if device_id is not None:
        q = q.filter(Message.device_id == device_id)
    if direction is not None:
        q = q.filter(Message.direction == direction)
    if msg_status is not None:
        q = q.filter(Message.status == msg_status)
    return q.order_by(Message.created_at.desc()).limit(limit).all()


@router.get("/messages/{message_id}", response_model=MessageOut)
def get_message(message_id: int, user: Annotated[User, Depends(get_current_user)],
                db: Annotated[Session, Depends(get_db)]) -> Message:
    msg = db.get(Message, message_id)
    if not msg or msg.device.owner_id != user.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Message not found")
    return msg


# --- Device-facing (X-API-Key) ---
@router.get("/device/messages/pending", response_model=list[MessageOut])
def pending_for_device(device: Annotated[Device, Depends(get_device_by_api_key)],
                       db: Annotated[Session, Depends(get_db)]) -> list[Message]:
    device.last_seen = datetime.now(timezone.utc)
    db.commit()
    return db.query(Message).filter(
        Message.device_id == device.id, Message.direction == Direction.OUTBOUND,
        Message.status == MessageStatus.PENDING
    ).order_by(Message.created_at).all()


@router.post("/device/messages/{message_id}/status", response_model=MessageOut)
def update_status(message_id: int, body: StatusUpdateRequest,
                  device: Annotated[Device, Depends(get_device_by_api_key)],
                  db: Annotated[Session, Depends(get_db)]) -> Message:
    msg = db.get(Message, message_id)
    if not msg or msg.device_id != device.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Message not found")
    msg.status = body.status
    msg.error = body.error
    db.commit()
    db.refresh(msg)
    event = _STATUS_EVENT.get(body.status)
    if event:
        _dispatch_webhooks(db, device.owner_id, event, msg)
    return msg


@router.post("/device/messages/inbound", response_model=MessageOut,
             status_code=status.HTTP_201_CREATED)
def record_inbound(body: InboundMessageRequest,
                   device: Annotated[Device, Depends(get_device_by_api_key)],
                   db: Annotated[Session, Depends(get_db)]) -> Message:
    device.last_seen = datetime.now(timezone.utc)
    msg = Message(device_id=device.id, direction=Direction.INBOUND,
                  phone_number=body.phone_number, text=body.text,
                  status=MessageStatus.RECEIVED)
    db.add(msg)
    db.commit()
    db.refresh(msg)
    _dispatch_webhooks(db, device.owner_id, "sms:received", msg)
    _forward_inbound_to_whatsapp(db, device.owner_id, msg)
    _forward_inbound_to_telegram(db, device.owner_id, msg.phone_number, msg.text, device.name)
    return msg


def _forward_inbound_to_whatsapp(db: Session, owner_id: int, msg: Message) -> None:
    """Premium: forward an inbound SMS to the owner's configured WhatsApp chat. Best-effort."""
    bridge = db.query(BridgeSettings).filter(
        BridgeSettings.owner_id == owner_id, BridgeSettings.enabled.is_(True)
    ).first()
    if not bridge or not bridge.forward_to_chat:
        return
    owner = db.get(User, owner_id)
    if not owner or (owner.tier != Tier.PREMIUM and owner.role.value != "SUPER_ADMIN"):
        return
    send_whatsapp(bridge.forward_to_chat, f"SMS from {msg.phone_number}:\n{msg.text}", bridge.waha_session)


def _forward_inbound_to_telegram(db: Session, owner_id: int, sender: str, body: str, device_name: str = "") -> None:
    """Forward an inbound SMS to Telegram if configured. Best-effort."""
    bridge = db.query(BridgeSettings).filter(
        BridgeSettings.owner_id == owner_id, BridgeSettings.telegram_enabled.is_(True)
    ).first()
    if not bridge or not bridge.telegram_chat_id or not bridge.telegram_bot_token:
        return
    telegram_forward_sms(sender, body, device_name, bridge.telegram_bot_token, bridge.telegram_chat_id)
