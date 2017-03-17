// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/stage_editor.h"
#include "app/ui/stage_view.h"

#include "app/app.h"
#include "app/app_menus.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/ui/main_window.h"
#include "app/ui/skin/skin_style_property.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/workspace.h"
#include "app/ui/workspace_tabs.h"
#include "app/ui_context.h"
#include "app/ui/editor/editor.h"
#include "base/bind.h"
#include "base/exception.h"
#include "ui/label.h"
#include "ui/resize_event.h"
#include "ui/system.h"
#include "ui/textbox.h"
#include "ui/view.h"
#include "app/context_access.h"
#include "app/document_access.h"
#include "app/document_range.h"
#include "doc/document_event.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/sprite.h"
#include "gfx/size.h"
#include "app/ui/editor/editor.h"
#include "app/console.h"
#include "she/surface.h"
#include "she/system.h"
#include "doc/conversion_she.h"
#include "doc/palette.h"
#include "app/color_utils.h"
#include "app/ui/timeline.h"
#include "app/ui/editor/moving_pixels_state.h"
#include "app/ui/editor/pixels_movement.h"
#include "app/ui/editor/play_state.h"
#include "app/ui/editor/scrolling_state.h"
#include "app/ui/editor/standby_state.h"
#include "app/ui/editor/zooming_state.h"
#include "ui/label.h"
#include "doc/frame_tag.h"

#define DEBUG_MSG App::instance()->mainWindow()->getStageView()->getDbgLabel()->setTextf

namespace app {

using namespace ui;
using namespace app::skin;

AppRender StageEditor::m_renderEngine;

StageEditor::StageEditor()
  : m_doublebuf(nullptr)
  , m_doublesur(nullptr)
  , m_bgPal(Palette::createGrayscale())
  , m_docPref("")
  , m_isPlaying(false)
  , m_frame(0)
  , m_isScrolling(false)
{
}

StageEditor::~StageEditor()
{
  m_doublesur->dispose();
  delete m_doublebuf;
}

void StageEditor::onResize(ui::ResizeEvent& ev)
{
  Widget::onResize(ev);

  if (m_doublebuf)
  {
    delete m_doublebuf;
  }
  m_doublebuf = Image::create(IMAGE_RGB, ev.bounds().w, ev.bounds().h);
  if (m_doublesur)
  {
    m_doublesur->dispose();
  }
  m_doublesur = she::instance()->createRgbaSurface(ev.bounds().w, ev.bounds().h);
}

frame_t StageEditor::frame()
{
  return m_frame;
}

void StageEditor::setFrame(frame_t frame)
{
  if (m_frame == frame)
    return;

  m_frame = frame;

  invalidate();
}

void StageEditor::play(const bool playOnce,
          const bool playAll)
{
}

void StageEditor::stop()
{

}

bool StageEditor::isPlaying() const
{
  return m_isPlaying;
}

FrameTag* StageEditor::currentFrameTag(Sprite * sprite)
{
  for (auto frameTag : sprite->frameTags())
  {
    if (frameTag->fromFrame() <= m_frame
      && m_frame <= frameTag->toFrame())
    {
      return frameTag;
    }
  }
  return nullptr;
}

void StageEditor::onPaint(ui::PaintEvent& ev)
{
  Graphics* g = ev.graphics();
  SkinTheme* theme = static_cast<SkinTheme*>(this->theme());

  drawBG(ev);

  if (m_doc == nullptr || m_doc->sprite() == nullptr) {
    return;
  }

  auto sprite = m_doc->sprite();
  auto frameTag = currentFrameTag(sprite);

  if (frameTag == nullptr) {
    drawSprite(g
      , gfx::Rect(0, 0, sprite->width(), sprite->height())
      , 0
      , 0
      , sprite
      , m_frame);

    return;
  }

  for (frame_t fr = frameTag->fromFrame(); fr <= frameTag->toFrame(); ++fr) {
    drawSprite(g
      , gfx::Rect(0, 0, sprite->width(), sprite->height())
      , sprite->frameRootPosition(fr).x
      , sprite->frameRootPosition(fr).y
      , sprite
      , fr);
  }
}

bool StageEditor::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kTimerMessage:
      break;

    case kMouseEnterMessage:
      break;

    case kMouseLeaveMessage:
      break;

    case kMouseDownMessage:
      {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
        if (mouseMsg->middle())
        {
          m_oldMousePos = mouseMsg->position();
          captureMouse();
          m_isScrolling = true;
          return true;
        }
      }
      break;

    case kMouseMoveMessage:
      if (m_isScrolling)
      {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
        View* view = View::getView(this);
        gfx::Point scroll = view->viewScroll();
        gfx::Point newPos = mouseMsg->position();

        scroll -= newPos - m_oldMousePos;
        m_oldMousePos = newPos;
        view->setViewScroll(scroll);
        //DEBUG_MSG("scroll x(%d) y(%d)", scroll.x, scroll.y);
        return true;
      }
      break;

    case kMouseUpMessage:
      if (m_isScrolling)
      {
        releaseMouse();
        m_isScrolling = false;
        return true;
      }
      break;

    case kDoubleClickMessage:
      break;

    case kTouchMagnifyMessage:
      break;

    case kKeyDownMessage:
      break;

    case kKeyUpMessage:
      break;

    case kFocusLeaveMessage:
      break;

    case kMouseWheelMessage:
      break;

    case kSetCursorMessage:
      break;
  }

  return Widget::onProcessMessage(msg);
}

void StageEditor::onSizeHint(SizeHintEvent& ev)
{
  gfx::Size sz(0, 0);
  if (m_doc != nullptr && m_doc->sprite() != nullptr)
  {
    sz.w = 500;
    sz.h = 500;
  }

  ev.setSizeHint(sz);
}

void StageEditor::drawBG(ui::PaintEvent& ev)
{
  Graphics* g = ev.graphics();

  m_renderEngine.setRefLayersVisiblity(false);
  m_renderEngine.setProjection(render::Projection());
  m_renderEngine.disableOnionskin();
  m_renderEngine.setBgType(render::BgType::TRANSPARENT);
  m_renderEngine.setProjection(m_proj);

  render::BgType bgType;
  gfx::Size tile;
  switch (m_docPref.bg.type()) {
    case app::gen::BgType::CHECKED_16x16:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(16, 16);
      break;
    case app::gen::BgType::CHECKED_8x8:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(8, 8);
      break;
    case app::gen::BgType::CHECKED_4x4:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(4, 4);
      break;
    case app::gen::BgType::CHECKED_2x2:
      bgType = render::BgType::CHECKED;
      tile = gfx::Size(2, 2);
      break;
    default:
      bgType = render::BgType::TRANSPARENT;
      break;
  }

  m_renderEngine.setBgType(bgType);
  m_renderEngine.setBgZoom(m_docPref.bg.zoom());
  m_renderEngine.setBgColor1(color_utils::color_for_image(m_docPref.bg.color1(), m_doublebuf->pixelFormat()));
  m_renderEngine.setBgColor2(color_utils::color_for_image(m_docPref.bg.color2(), m_doublebuf->pixelFormat()));
  m_renderEngine.setBgCheckedSize(tile);

  //m_renderEngine.setupBackground(m_doc, m_doublebuf->pixelFormat());
  m_renderEngine.renderBackground(m_doublebuf,
    gfx::Clip(0, 0, -m_pos.x, -m_pos.y,
      m_doublebuf->width(), m_doublebuf->height()));

  convert_image_to_surface(m_doublebuf, m_bgPal,
    m_doublesur, 0, 0, 0, 0, m_doublebuf->width(), m_doublebuf->height());
  g->blit(m_doublesur, 0, 0, 0, 0, m_doublesur->width(), m_doublesur->height()); 
}

void StageEditor::drawSprite(ui::Graphics* g
  , const gfx::Rect& spriteRectToDraw
  , int dx
  , int dy
  , Sprite * sprite
  , frame_t frame)
{
  // Clip from sprite and apply zoom
  gfx::Rect rc = sprite->bounds().createIntersection(spriteRectToDraw);
  rc = m_proj.apply(rc);

  int dest_x = dx + m_padding.x + rc.x + clientBounds().center().x;
  int dest_y = dy + m_padding.y + rc.y + clientBounds().center().y;

  // Clip from graphics/screen
  const gfx::Rect& clip = g->getClipBounds();
  if (dest_x < clip.x) {
    rc.x += clip.x - dest_x;
    rc.w -= clip.x - dest_x;
    dest_x = clip.x;
  }
  if (dest_y < clip.y) {
    rc.y += clip.y - dest_y;
    rc.h -= clip.y - dest_y;
    dest_y = clip.y;
  }
  if (dest_x+rc.w > clip.x+clip.w) {
    rc.w = clip.x+clip.w-dest_x;
  }
  if (dest_y+rc.h > clip.y+clip.h) {
    rc.h = clip.y+clip.h-dest_y;
  }

  if (rc.isEmpty())
    return;

  auto renderBuf = Editor::getRenderImageBuffer();
  // Generate the rendered image
  if (!renderBuf)
    renderBuf.reset(new doc::ImageBuffer());

  base::UniquePtr<Image> rendered(NULL);
  try {
    // Generate a "expose sprite pixels" notification. This is used by
    // tool managers that need to validate this region (copy pixels from
    // the original cel) before it can be used by the RenderEngine.
    {
      gfx::Rect expose = m_proj.remove(rc);

      // If the zoom level is less than 100%, we add extra pixels to
      // the exposed area. Those pixels could be shown in the
      // rendering process depending on each cel position.
      // E.g. when we are drawing in a cel with position < (0,0)
      if (m_proj.scaleX() < 1.0)
        expose.enlargeXW(int(1./m_proj.scaleX()));
      // If the zoom level is more than %100 we add an extra pixel to
      // expose just in case the zoom requires to display it.  Note:
      // this is really necessary to avoid showing invalid destination
      // areas in ToolLoopImpl.
      else if (m_proj.scaleX() > 1.0)
        expose.enlargeXW(1);

      if (m_proj.scaleY() < 1.0)
        expose.enlargeYH(int(1./m_proj.scaleY()));
      else if (m_proj.scaleY() > 1.0)
        expose.enlargeYH(1);

      m_doc->notifyExposeSpritePixels(sprite, gfx::Region(expose));
    }

    // Create a temporary RGBA bitmap to draw all to it
    rendered.reset(Image::create(IMAGE_RGB, rc.w, rc.h, renderBuf));

    m_renderEngine.setRefLayersVisiblity(true);
    //m_renderEngine.setSelectedLayer(m_layer);
    m_renderEngine.setNonactiveLayersOpacity(255);
    m_renderEngine.setProjection(m_proj);
    m_renderEngine.setupBackground(m_doc, rendered->pixelFormat());
    m_renderEngine.disableOnionskin();
    m_renderEngine.setBgType(render::BgType::TRANSPARENT);

    m_renderEngine.renderSprite(
      rendered, sprite, frame, gfx::Clip(0, 0, rc));

    m_renderEngine.removeExtraImage();
  }
  catch (const std::exception& e) {
    Console::showException(e);
  }

  if (rendered) {
    // Convert the render to a she::Surface
    static she::Surface* tmp;
    if (!tmp || tmp->width() < rc.w || tmp->height() < rc.h) {
      if (tmp)
        tmp->dispose();

      tmp = she::instance()->createRgbaSurface(rc.w, rc.h);
    }

    if (tmp->nativeHandle()) {
      convert_image_to_surface(rendered, sprite->palette(frame),
        tmp, 0, 0, 0, 0, rc.w, rc.h);

      g->drawRgbaSurface(tmp, dest_x, dest_y);
    }
  }
}


} // namespace app
