#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/set_pivot.h"

#include "app/doc.h"
#include "app/doc_event.h"
#include "app/doc_observer.h"
#include "doc/sprite.h"

namespace app {
namespace cmd {

using namespace doc;

SetPivot::SetPivot(Sprite* sprite, gfx::PointF pivot)
  : WithSprite(sprite)
  , m_oldPivot(sprite->pivot())
  , m_newPivot(pivot)
{
}

void SetPivot::onExecute()
{
  Sprite* spr = sprite();
  spr->setPivot(m_newPivot);
  spr->incrementVersion();
}

void SetPivot::onUndo()
{
  Sprite* spr = sprite();
  spr->setPivot(m_oldPivot);
  spr->incrementVersion();
}

void SetPivot::onFireNotifications()
{
  Sprite* sprite = this->sprite();
  Doc* doc = static_cast<Doc*>(sprite->document());
  DocEvent ev(doc);
  ev.sprite(sprite);
}

} // namespace cmd
} // namespace app
