"""WhatsApp bridge helpers: WAHA client + keyword transcoding parser."""
import logging

import httpx

from .config import get_settings

logger = logging.getLogger("whatsapp")
settings = get_settings()


def waha_configured() -> bool:
    return bool(settings.waha_base_url and settings.waha_api_key)


def send_whatsapp(chat_id: str, text: str, session: str | None = None) -> bool:
    """Send a WhatsApp message via WAHA. Best-effort — returns success boolean, never raises."""
    if not waha_configured():
        logger.info("WAHA not configured; skipping send to %s", chat_id)
        return False
    try:
        resp = httpx.post(
            f"{settings.waha_base_url.rstrip('/')}/api/sendText",
            headers={"X-Api-Key": settings.waha_api_key},
            json={"session": session or settings.waha_session, "chatId": chat_id, "text": text},
            timeout=10,
        )
        return resp.status_code < 300
    except Exception as exc:  # noqa: BLE001
        logger.warning("WAHA send failed: %s", exc)
        return False


def parse_keyword_command(text: str) -> tuple[str, str] | None:
    """Parse a WhatsApp→SMS command.

    Grammar:  <trigger> <E164_recipient> <message body...>
    e.g.      @sms +447700900123 Hello there

    Returns (recipient, body) or None if it doesn't match the trigger / is malformed.
    """
    trigger = settings.whatsapp_trigger_keyword
    stripped = text.strip()
    if not stripped.lower().startswith(trigger.lower()):
        return None
    rest = stripped[len(trigger):].strip()
    parts = rest.split(maxsplit=1)
    if len(parts) < 2:
        return None
    recipient, body = parts[0], parts[1].strip()
    # Minimal recipient sanity check (E.164-ish).
    digits = recipient.lstrip("+")
    if not digits.isdigit() or not (6 <= len(digits) <= 15) or not body:
        return None
    return recipient, body


def normalize_chat_to_number(chat_id: str) -> str:
    """WAHA chat ids look like '447700900123@c.us' → '+447700900123'."""
    local = chat_id.split("@", 1)[0]
    return f"+{local}" if local.isdigit() else local
