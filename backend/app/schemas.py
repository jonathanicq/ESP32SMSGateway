"""Pydantic request/response schemas."""
from datetime import datetime

from pydantic import BaseModel, ConfigDict, EmailStr, Field

from .models import Direction, MessageStatus, Role, Tier


# --- Auth ---
class RegisterRequest(BaseModel):
    email: EmailStr
    password: str = Field(min_length=8)


class LoginRequest(BaseModel):
    email: EmailStr
    password: str


class TokenPair(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"


class RefreshRequest(BaseModel):
    refresh_token: str


class UserOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    email: EmailStr
    role: Role
    tier: Tier
    is_active: bool


# --- Devices ---
class DeviceCreate(BaseModel):
    name: str = Field(min_length=1, max_length=120)


class DeviceOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    name: str
    api_key: str
    last_seen: datetime | None = None
    created_at: datetime


# --- Messages ---
class SendMessageRequest(BaseModel):
    device_id: int
    phone_number: str = Field(min_length=3, max_length=32)
    text: str = Field(min_length=1)


class MessageOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    device_id: int
    direction: Direction
    phone_number: str
    text: str
    status: MessageStatus
    error: str | None = None
    created_at: datetime
    updated_at: datetime


class InboundMessageRequest(BaseModel):
    phone_number: str = Field(min_length=3, max_length=32)
    text: str = Field(min_length=1)


class StatusUpdateRequest(BaseModel):
    status: MessageStatus
    error: str | None = None


# --- Webhooks ---
class WebhookCreate(BaseModel):
    url: str = Field(min_length=8, max_length=500)
    event: str = Field(pattern=r"^sms:(received|sent|delivered|failed)$")


class WebhookOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    url: str
    event: str
    is_active: bool
    created_at: datetime


# --- Device pairing ---
class PairStartRequest(BaseModel):
    device_name: str = Field(default="Android device", min_length=1, max_length=120)


class PairStartResponse(BaseModel):
    claim_code: str
    poll_token: str
    expires_in: int


class PairPollRequest(BaseModel):
    poll_token: str


class PairPollResponse(BaseModel):
    status: str               # pending | linked | expired
    api_key: str | None = None


class ClaimRequest(BaseModel):
    claim_code: str = Field(min_length=4, max_length=12)
    name: str | None = Field(default=None, max_length=120)


# --- WhatsApp bridge (premium) ---
class BridgeSettingsIn(BaseModel):
    enabled: bool = False
    waha_session: str = Field(default="default", max_length=80)
    forward_to_chat: str | None = Field(default=None, max_length=120)
    send_device_id: int | None = None


class BridgeSettingsOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)
    id: int
    enabled: bool
    waha_session: str
    forward_to_chat: str | None
    send_device_id: int | None
    created_at: datetime
