"""Webhook registration (user-facing)."""
from typing import Annotated

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import User, Webhook
from ..schemas import WebhookCreate, WebhookOut
from ..security import get_current_user

router = APIRouter(prefix="/webhooks", tags=["webhooks"])


@router.post("", response_model=WebhookOut, status_code=status.HTTP_201_CREATED)
def create_webhook(body: WebhookCreate, user: Annotated[User, Depends(get_current_user)],
                   db: Annotated[Session, Depends(get_db)]) -> Webhook:
    hook = Webhook(owner_id=user.id, url=body.url, event=body.event)
    db.add(hook)
    db.commit()
    db.refresh(hook)
    return hook


@router.get("", response_model=list[WebhookOut])
def list_webhooks(user: Annotated[User, Depends(get_current_user)],
                  db: Annotated[Session, Depends(get_db)]) -> list[Webhook]:
    return db.query(Webhook).filter(Webhook.owner_id == user.id).all()


@router.delete("/{webhook_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_webhook(webhook_id: int, user: Annotated[User, Depends(get_current_user)],
                   db: Annotated[Session, Depends(get_db)]) -> None:
    hook = db.get(Webhook, webhook_id)
    if not hook or hook.owner_id != user.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Webhook not found")
    db.delete(hook)
    db.commit()
