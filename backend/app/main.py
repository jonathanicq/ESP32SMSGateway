"""FastAPI application entrypoint."""
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .config import get_settings
from .database import Base, SessionLocal, engine
from .exec_logging import log_execution
from .routers import admin, auth, devices, health, messages, pairing, telegram, webhooks, whatsapp
from .seed import seed_super_admin

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("main")
settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    Base.metadata.create_all(bind=engine)
    with SessionLocal() as db:
        seed_super_admin(db)
    log_execution(event="startup", status="success",
                  detail=f"v{settings.app_version} env={settings.app_env}")
    logger.info("android-sms-gateway backend started (v%s, %s)",
                settings.app_version, settings.app_env)
    yield


app = FastAPI(title="Android SMS Gateway", version=settings.app_version, lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["https://sms-gateway.thecoordinates.app",
                   "https://test.sms-gateway.thecoordinates.app",
                   "http://localhost:3050", "http://localhost:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

api = "/api"
app.include_router(health.router, prefix=api)
app.include_router(auth.router, prefix=api)
app.include_router(devices.router, prefix=api)
app.include_router(pairing.router, prefix=api)
app.include_router(messages.router, prefix=api)
app.include_router(webhooks.router, prefix=api)
app.include_router(whatsapp.router, prefix=api)
app.include_router(telegram.router, prefix=api)
app.include_router(admin.router, prefix=api)
