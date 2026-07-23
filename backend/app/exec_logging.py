"""Best-effort execution logging to makebdd.execution_logs (non-negotiable #10)."""
import logging

from sqlalchemy import create_engine, text

from .config import get_settings

logger = logging.getLogger("exec_logging")
settings = get_settings()


def log_execution(event: str, status: str = "success", detail: str = "") -> None:
    """Write one row to makebdd.execution_logs. Never raises — logging must not break startup."""
    if not settings.exec_log_db_url:
        logger.info("exec-log skipped (EXEC_LOG_DB_URL not set): %s/%s %s", event, status, detail)
        return
    try:
        engine = create_engine(settings.exec_log_db_url, pool_pre_ping=True)
        with engine.begin() as conn:
            conn.execute(
                text("INSERT INTO execution_logs (app_name, event, status, detail, created_at) "
                     "VALUES (:app, :event, :status, :detail, NOW())"),
                {"app": settings.exec_log_app_name, "event": event,
                 "status": status, "detail": detail[:1000]},
            )
        engine.dispose()
    except Exception as exc:  # noqa: BLE001 — best-effort, must not break the app
        logger.warning("exec-log write failed: %s", exc)
