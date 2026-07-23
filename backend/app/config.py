"""Application settings loaded from environment variables."""
from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    # Environment
    app_env: str = "development"
    app_version: str = "0.1.0"
    git_commit: str = "unknown"

    # Database
    database_url: str = "sqlite:///./sms_gateway.db"

    # Auth / JWT
    jwt_secret: str = "change-me-in-production"
    jwt_algorithm: str = "HS256"
    access_token_ttl_minutes: int = 60
    refresh_token_ttl_days: int = 30
    impersonation_ttl_minutes: int = 60

    # Master account (MANDATORY — seeded on first startup)
    super_admin_email: str = "admin@thecoordinates.xyz"
    super_admin_password: str = "ChangeMe123!"

    # Execution logging (makebdd.execution_logs) — best-effort
    exec_log_db_url: str | None = None
    exec_log_app_name: str = "android-sms-gateway"

    # WhatsApp bridge (premium) — WAHA (https://waha.devlike.pro)
    waha_base_url: str | None = None          # e.g. http://192.168.0.135:3099
    waha_api_key: str | None = None
    waha_session: str = "default"
    whatsapp_trigger_keyword: str = "@sms"    # WhatsApp→SMS transcoding trigger
    whatsapp_webhook_token: str = "change-me-webhook-token"  # shared secret for the WAHA webhook

    # Telegram bridge — Telegram Bot API
    telegram_bot_token: str | None = None          # Bot token from @BotFather
    telegram_webhook_chat_id: str | None = None    # Default chat ID for webhooks
    telegram_sms_trigger_keyword: str = "/sms"     # Telegram→SMS transcoding trigger

    @property
    def is_production(self) -> bool:
        return self.app_env == "production"


@lru_cache
def get_settings() -> Settings:
    return Settings()
