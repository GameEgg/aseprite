// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_STAGE_VIEW_H_INCLUDED
#define APP_UI_STAGE_VIEW_H_INCLUDED
#pragma once

#include "app/app_render.h"
#include "app/ui/tabs.h"
#include "app/ui/workspace_view.h"
#include "app/ui/editor/editor.h"
#include "app/ui/playable.h"
#include "ui/box.h"
#include "app/pref/preferences.h"
#include "app/ui/editor/editor_state.h"

#include "stage_view.xml.h"

namespace ui {
  class Label;
  class View;
}
namespace doc{
  class Palette;
}

namespace app {
  using namespace doc;

  class StageEditor;

  class StageView : public gen::StageView
                 , public TabView
                 , public WorkspaceView
  {
  public:
    StageView();
    ~StageView();

    void updateUsingEditor(Editor* editor);

    // TabView implementation
    std::string getTabText() override;
    TabIcon getTabIcon() override;

    // WorkspaceView implementation
    ui::Widget* getContentWidget() override { return this; }
    bool onCloseView(Workspace* workspace, bool quitting) override;
    void onTabPopup(Workspace* workspace) override;
    void onWorkspaceViewSelected() override;

    ui::Label* getDbgLabel() {return m_dbgLabel;}
    ui::Label* getPositionLabel() {return m_positionLabel;}

  protected:
    void onResize(ui::ResizeEvent& ev) override;

  private:
    StageEditor* m_stageEditor;
    Editor* m_relatedEditor;
    Document* m_doc;
    ui::Label* m_dbgLabel;
    ui::Label* m_positionLabel;
  };

} // namespace app

#endif
