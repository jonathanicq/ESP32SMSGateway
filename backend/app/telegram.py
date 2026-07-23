"""Telegram bot integration: send SMS messages to Telegram."""
import logging

import httpx

from .config import get_settings

logger = logging.getLogger("telegram")
settings = get_settings()


def telegram_configured(token: str | None = None, chat_id: str | None = None) -> bool:
    """Check if Telegram is configured (per-user or globally)."""
    if token and chat_id:
        return bool(token and chat_id)
    return bool(settings.telegram_bot_token and settings.telegram_webhook_chat_id)


def send_telegram(message: str, token: str | None = None, chat_id: str | None = None) -> bool:
    """Send a message to Telegram via Bot API. Best-effort — returns success boolean."""
    use_token = token or settings.telegram_bot_token
    use_chat_id = chat_id or settings.telegram_webhook_chat_id

    if not telegram_configured(use_token, use_chat_id):
        logger.info("Telegram not configured; skipping send")
        return False

    try:
        resp = httpx.post(
            f"https://api.telegram.org/bot{use_token}/sendMessage",
            json={"chat_id": use_chat_id, "text": message, "parse_mode": "HTML"},
            timeout=10,
        )
        success = resp.status_code == 200
        if not success:
            logger.warning("Telegram send failed: %s", resp.text)
        return success
    except Exception as exc:  # noqa: BLE001
        logger.warning("Telegram send failed: %s", exc)
        return False


def format_sms_to_telegram(sender: str, body: str) -> str:
    """Format an inbound SMS for Telegram display."""
    return f"📱 <b>SMS from {sender}</b>\n\n{body}"


def format_sms_to_telegram_html(sender: str, body: str, device_name: str = "") -> str:
    """Format an inbound SMS for Telegram with device info."""
    device_info = f" ({device_name})" if device_name else ""
    escaped_body = body.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    return f"📱 <b>SMS{device_info}</b>\n<b>From:</b> {sender}\n\n<code>{escaped_body}</code>"


def parse_keyword_command(text: str, trigger: str | None = None) -> tuple[str, str] | None:
    """Parse a Telegram→SMS command.

    Grammar:  <trigger> <E164_recipient> <message body...>
    e.g.      /sms +447700900123 Hello there

    Returns (recipient, body) or None if it doesn't match the trigger / is malformed.
    """
    _trigger = trigger or settings.telegram_sms_trigger_keyword
    stripped = text.strip()
    if not stripped.lower().startswith(_trigger.lower()):
        return None
    rest = stripped[len(_trigger):].strip()
    parts = rest.split(maxsplit=1)
    if len(parts) < 2:
        return None
    recipient, body = parts[0], parts[1].strip()
    # Minimal recipient sanity check (E.164-ish).
    digits = recipient.lstrip("+")
    if not digits.isdigit() or not (6 <= len(digits) <= 15) or not body:
        return None
    return recipient, body
