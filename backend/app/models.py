"""Database models: User, Device, Message, Webhook."""
import enum
import secrets
from datetime import datetime, timezone

from sqlalchemy import DateTime, Enum, ForeignKey, String, Text, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .database import Base


def _utcnow() -> datetime:
    return datetime.now(timezone.utc)


class Role(str, enum.Enum):
    USER = "USER"
    SUPER_ADMIN = "SUPER_ADMIN"


class Tier(str, enum.Enum):
    FREE = "free"
    PREMIUM = "premium"


class Direction(str, enum.Enum):
    OUTBOUND = "outbound"
    INBOUND = "inbound"


class MessageStatus(str, enum.Enum):
    PENDING = "pending"
    SENT = "sent"
    DELIVERED = "delivered"
    FAILED = "failed"
    RECEIVED = "received"


class User(Base):
    __tablename__ = "users"

    id: Mapped[int] = mapped_column(primary_key=True)
    email: Mapped[str] = mapped_column(String(255), unique=True, index=True)
    hashed_password: Mapped[str] = mapped_column(String(255))
    role: Mapped[Role] = mapped_column(Enum(Role), default=Role.USER)
    tier: Mapped[Tier] = mapped_column(Enum(Tier), default=Tier.FREE)
    is_active: Mapped[bool] = mapped_column(default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)

    devices: Mapped[list["Device"]] = relationship(back_populates="owner")
    webhooks: Mapped[list["Webhook"]] = relationship(back_populates="owner")


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[int] = mapped_column(primary_key=True)
    name: Mapped[str] = mapped_column(String(120))
    api_key: Mapped[str] = mapped_column(String(64), unique=True, index=True,
                                         default=lambda: secrets.token_urlsafe(32))
    owner_id: Mapped[int] = mapped_column(ForeignKey("users.id"))
    last_seen: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)

    owner: Mapped["User"] = relationship(back_populates="devices")
    messages: Mapped[list["Message"]] = relationship(back_populates="device")


class Message(Base):
    __tablename__ = "messages"

    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[int] = mapped_column(ForeignKey("devices.id"), index=True)
    direction: Mapped[Direction] = mapped_column(Enum(Direction))
    phone_number: Mapped[str] = mapped_column(String(32), index=True)
    text: Mapped[str] = mapped_column(Text)
    status: Mapped[MessageStatus] = mapped_column(Enum(MessageStatus),
                                                  default=MessageStatus.PENDING, index=True)
    error: Mapped[str | None] = mapped_column(String(255), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow, index=True)
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow,
                                                 onupdate=_utcnow)

    device: Mapped["Device"] = relationship(back_populates="messages")


class PairingRequest(Base):
    """Device pairing: the app creates one, shows the claim_code; the user claims it in the
    backoffice; the app then polls with poll_token and receives the device api_key."""
    __tablename__ = "pairing_requests"

    id: Mapped[int] = mapped_column(primary_key=True)
    claim_code: Mapped[str] = mapped_column(String(12), unique=True, index=True)
    poll_token: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    device_name: Mapped[str] = mapped_column(String(120))
    status: Mapped[str] = mapped_column(String(20), default="pending")  # pending | linked
    api_key: Mapped[str | None] = mapped_column(String(64), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)
    expires_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))


class BridgeSettings(Base):
    """Per-user SMS bridge config (WhatsApp + Telegram). One row per owner."""
    __tablename__ = "bridge_settings"

    id: Mapped[int] = mapped_column(primary_key=True)
    owner_id: Mapped[int] = mapped_column(ForeignKey("users.id"), unique=True, index=True)
    enabled: Mapped[bool] = mapped_column(default=False)
    # WAHA session this bridge listens on (maps inbound WhatsApp → this owner).
    waha_session: Mapped[str] = mapped_column(String(80), default="default", index=True)
    # WhatsApp chat to forward inbound SMS to (e.g. "12345@c.us" or group id).
    forward_to_chat: Mapped[str | None] = mapped_column(String(120), nullable=True)
    # Telegram settings
    telegram_enabled: Mapped[bool] = mapped_column(default=False)
    telegram_bot_token: Mapped[str | None] = mapped_column(String(256), nullable=True)
    telegram_chat_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    # Device used to send WhatsApp→SMS or Telegram→SMS transcoded messages.
    send_device_id: Mapped[int | None] = mapped_column(ForeignKey("devices.id"), nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)


class Webhook(Base):
    __tablename__ = "webhooks"

    id: Mapped[int] = mapped_column(primary_key=True)
    owner_id: Mapped[int] = mapped_column(ForeignKey("users.id"), index=True)
    url: Mapped[str] = mapped_column(String(500))
    event: Mapped[str] = mapped_column(String(40))  # e.g. sms:received, sms:sent, sms:delivered, sms:failed
    is_active: Mapped[bool] = mapped_column(default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)

    owner: Mapped["User"] = relationship(back_populates="webhooks")
