{%MainUnit gtk2wscomctrls.pp}
{
 *****************************************************************************
 *                                                                           *
 *  See the file COPYING.modifiedLGPL.txt, included in this distribution,    *
 *  for details about the copyright.                                         *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                     *
 *                                                                           *
 *****************************************************************************
}

const
  GtkPositionTypeMap: array[TTabPosition] of TGtkPositionType =
  (
{ tpTop    } GTK_POS_TOP,
{ tpBottom } GTK_POS_BOTTOM,
{ tpLeft   } GTK_POS_LEFT,
{ tpRight  } GTK_POS_RIGHT
  );

  LCL_NotebookManualPageSwitchKey = 'lcl_manual_page_switch';


type
  GtkNotebookPressEventProc = function (widget:PGtkWidget; event:PGdkEventButton):gboolean; cdecl;
  
var
  OldNoteBookButtonPress: GtkNotebookPressEventProc = nil;

// this was created as a workaround of a tnotebook eating rightclick of custom controls
function Notebook_Button_Press(widget:PGtkWidget; event:PGdkEventButton):gboolean; cdecl;
begin
  Result := True;
  if gtk_get_event_widget(PGdkEvent(event)) <> widget then exit;
  if OldNoteBookButtonPress = nil then exit;
  Result := OldNoteBookButtonPress(widget, event);
end;

procedure HookNoteBookClass;
var
  WidgetClass: PGtkWidgetClass;
begin
  WidgetClass := GTK_WIDGET_CLASS(gtk_type_class(gtk_notebook_get_type));

  OldNoteBookButtonPress := GtkNotebookPressEventProc(WidgetClass^.button_press_event);
  WidgetClass^.button_press_event := @Notebook_Button_Press;
end;

{ TGtk2WSCustomTabControl }

function NotebookPageRealToLCLIndex(const ATabControl: TCustomTabControl; AIndex: integer): integer;
var
  I: Integer;
begin
  Result := AIndex;
  if csDesigning in ATabControl.ComponentState then exit;
  I := 0;
  while (I < ATabControl.PageCount) and (I <= Result) do
  begin
    if not ATabControl.Page[I].TabVisible then Inc(Result);
    Inc(I);
  end;
end;

function GtkRestoreFocusFix(AGtkWidget: Pointer): gboolean; cdecl;
begin
  Result := AGtkWidget <> nil;
  if AGtkWidget <> nil then
  begin
    GTK_WIDGET_SET_FLAGS(PGtkWidget(AGtkWidget), GTK_CAN_FOCUS);
    g_idle_remove_by_data(AGtkWidget);
  end;
end;

function GtkWSNotebook_AfterSwitchPage(widget: PGtkWidget; {%H-}page: Pgtkwidget; pagenum: integer; data: gPointer): GBoolean; cdecl;
var
  Mess: TLMNotify;
  NMHdr: tagNMHDR;
  Info: PWidgetInfo;
  ACtl: TWinControl;
  AParentForm: TCustomForm;
  i: Integer;
  LCLPageIndex: Integer;
  Pg: TCustomPage;
  ChildWidget: PGtkWidget;
begin
  Result := CallBackDefaultReturn;
  // then send the new page
  FillChar(Mess{%H-}, SizeOf(Mess), 0);
  Mess.Msg := LM_NOTIFY;
  FillChar(NMHdr{%H-}, SizeOf(NMHdr), 0);
  NMHdr.code := TCN_SELCHANGE;
  NMHdr.hwndFrom := {%H-}PtrUInt(widget);
  LCLPageIndex := NotebookPageRealToLCLIndex(TCustomTabControl(Data), pagenum);  //use this to set pageindex to the correct page.
  NMHdr.idFrom := LCLPageIndex;
  Mess.NMHdr := @NMHdr;
  DeliverMessage(Data, Mess);

  // code below is fix for issue #20493
  Info := GetWidgetInfo(Widget);
  if wwiTabWidgetFocusCheck in Info^.Flags then
  begin
    Exclude(Info^.Flags, wwiTabWidgetFocusCheck);

    if LCLPageIndex = -1 then
      exit;

    ACtl := TWinControl(Data);
    AParentForm := GetParentForm(ACtl);
    if Assigned(AParentForm) then
    begin
      // 1st we must find focused control (if any)
      ACtl := nil;
      if (LCLPageIndex >= 0) and (LCLPageIndex < TCustomTabControl(Data).PageCount) then
        Pg := TCustomTabControl(Data).Page[LCLPageIndex]
      else
        Pg := nil;
      if Assigned(Pg) then
      begin
        for i := 0 to Pg.ControlCount - 1 do
        begin
          if (pg.Controls[i] is TWinControl) and
            (TWinControl(pg.Controls[i]).Focused) then
          begin
            ACtl := TWinControl(pg.Controls[i]);
            break;
          end;
        end;
      end;
      if (ACtl = nil) and (Pg <> nil) then
        ACtl := AParentForm.ActiveControl;
    end else
      ACtl := nil;

    if (ACtl <> nil) and (ACtl <> TWinControl(Data)) then
    begin
      // DebugLn('ActiveCtl is ',ACtl.ClassName,':',ACtl.Name);
      // do not focus tab by mouse click if we already have active control
      GTK_WIDGET_UNSET_FLAGS(Widget, GTK_CAN_FOCUS);
      Pg := TCustomTabControl(Data).Page[LCLPageIndex];
      for i := 0 to Pg.ControlCount - 1 do
      begin
        // we must prevent gtkWidget to acquire focus by gtk (eg. GtkButton)
        if (Pg.Controls[i] is TWinControl) and (Pg.Controls[i] <> ACtl) then
        begin
          Info := GetWidgetInfo({%H-}PGtkWidget(TWinControl(Pg.Controls[i]).Handle));
          if Info <> nil then
          begin
            if Info^.CoreWidget <> nil then
              ChildWidget := Info^.CoreWidget
            else
              ChildWidget := Info^.ClientWidget;
            GTK_WIDGET_UNSET_FLAGS(ChildWidget, GTK_CAN_FOCUS);
            g_idle_add(@GtkRestoreFocusFix, ChildWidget);
          end;
        end;
      end;
      g_idle_add(@GtkRestoreFocusFix, Widget);
    end;
  end;
end;

function GtkWSNotebook_SwitchPage(widget: PGtkWidget; {%H-}page: Pgtkwidget; pagenum: integer; data: gPointer): GBoolean; cdecl;
var
  Mess: TLMNotify;
  NMHdr: tagNMHDR;
  IsManual: Boolean;
begin
  Result := CallBackDefaultReturn;
  EventTrace('switch-page', data);
  UpdateNoteBookClientWidget(TObject(Data));

  // remove flag
  IsManual := g_object_get_data(PGObject(Widget), LCL_NotebookManualPageSwitchKey) <> nil;
  if IsManual then
    g_object_set_data(PGObject(Widget), LCL_NotebookManualPageSwitchKey, nil);
  if PGtkNotebook(Widget)^.cur_page = nil then // for windows compatibility
    Exit;

  // gtkswitchpage is called before the switch
  if not IsManual then
  begin
    // send first the TCN_SELCHANGING to ask if switch is allowed
    FillChar(Mess{%H-}, SizeOf(Mess), 0);
    Mess.Msg := LM_NOTIFY;
    FillChar(NMHdr{%H-}, SizeOf(NMHdr), 0);
    NMHdr.code := TCN_SELCHANGING;
    NMHdr.hwndFrom := {%H-}PtrUInt(widget);
    NMHdr.idFrom := NotebookPageRealToLCLIndex(TCustomTabControl(Data), pagenum);  //use this to set pageindex to the correct page.
    Mess.NMHdr := @NMHdr;
    Mess.Result := 0;
    DeliverMessage(Data, Mess);
    if Mess.Result <> 0 then
    begin
      g_signal_stop_emission_by_name(PGObject(Widget), 'switch-page');
      Result := not CallBackDefaultReturn;
      Exit;
    end;
  end;

end;

class procedure TGtk2WSCustomTabControl.SetCallbacks(
  const AGtkWidget: PGtkWidget; const AWidgetInfo: PWidgetInfo);
begin
  TGtk2WSWinControl.SetCallbacks(PGtkObject(AGtkWidget), TComponent(AWidgetInfo^.LCLObject));
  ConnectSignal(PGtkObject(AGtkWidget), 'switch_page', @GtkWSNotebook_SwitchPage, AWidgetInfo^.LCLObject);
  ConnectSignalAfter(PGtkObject(AGtkWidget), 'switch_page', @GtkWSNotebook_AfterSwitchPage, AWidgetInfo^.LCLObject);
end;

class function TGtk2WSCustomTabControl.CreateTTabControlHandle(
  const AWinControl: TWinControl; const AParams: TCreateParams): HWND;
var
  Widget: PGtkWidget;
  WidgetInfo: PWidgetInfo;
  Allocation: TGTKAllocation;
begin
  Widget := GTK2WidgetSet.CreateAPIWidget(AWinControl);
  {$IFDEF DebugLCLComponents}
  DebugGtkWidgets.MarkCreated(Widget, dbgsName(AWinControl));
  {$ENDIF}

  Result := THandle({%H-}PtrUInt(Widget));
  if Result = 0 then Exit;

  WidgetInfo := GetWidgetInfo(Widget); // Widget info already created in CreateAPIWidget
  WidgetInfo^.Style := AParams.Style;
  WidgetInfo^.ExStyle := AParams.ExStyle;
  WidgetInfo^.WndProc := {%H-}PtrUInt(AParams.WindowClass.lpfnWndProc);

  // set allocation
  Allocation.X := AParams.X;
  Allocation.Y := AParams.Y;
  Allocation.Width := AParams.Width;
  Allocation.Height := AParams.Height;
  gtk_widget_size_allocate(Widget, @Allocation);

  Set_RC_Name(AWinControl, Widget);

  g_object_set_data(PGObject(WidgetInfo^.CoreWidget),'lcl_ttabcontrol', WidgetInfo^.CoreWidget);

  TGtk2WSWinControl.SetCallbacks(GTK_OBJECT(Widget), AWinControl);

  g_signal_connect_after(GTK_SCROLLED_WINDOW(Widget)^.hscrollbar, 'change-value',
    TGCallback(@Gtk2RangeScrollCB), WidgetInfo);
  g_signal_connect_after(GTK_SCROLLED_WINDOW(Widget)^.vscrollbar, 'change-value',
    TGCallback(@Gtk2RangeScrollCB), WidgetInfo);
  g_signal_connect(GTK_SCROLLED_WINDOW(Widget)^.hscrollbar, 'button-press-event',
    TGCallback(@Gtk2RangeScrollPressCB), WidgetInfo);
  g_signal_connect(GTK_SCROLLED_WINDOW(Widget)^.hscrollbar, 'button-release-event',
    TGCallback(@Gtk2RangeScrollReleaseCB), WidgetInfo);
    g_signal_connect(GTK_SCROLLED_WINDOW(Widget)^.vscrollbar, 'button-press-event',
    TGCallback(@Gtk2RangeScrollPressCB), WidgetInfo);
  g_signal_connect(GTK_SCROLLED_WINDOW(Widget)^.vscrollbar, 'button-release-event',
    TGCallback(@Gtk2RangeScrollReleaseCB), WidgetInfo);

  g_signal_connect(Widget, 'scroll-event', TGCallback(@Gtk2ScrolledWindowScrollCB), WidgetInfo);
end;

class function TGtk2WSCustomTabControl.CreateHandle(const AWinControl: TWinControl;
  const AParams: TCreateParams): HWND;
var
  AWidget: PGtkNoteBook;
  WidgetInfo: PWidgetInfo;
begin

  if (AWinControl is TTabControl) then
  begin
    {$IFDEF NOTEBOOK_DEBUG}
    DebugLn(['TGtk2WSCustomTabControl.CreateHandle CREATING TTABCONTROL !!! ',DbgSName(AWinControl)]);
    {$ENDIF}
    Result := CreateTTabControlHandle(AWinControl, AParams);
    exit;
  end;

  {$IFDEF NOTEBOOK_DEBUG}
  DebugLn(['TGtk2WSCustomTabControl.CreateHandle ',DbgSName(AWinControl)]);
  {$ENDIF}
  if OldNoteBookButtonPress = nil then
    HookNoteBookClass;


  AWidget := PGtkNoteBook(gtk_notebook_new());
  WidgetInfo := CreateWidgetInfo(AWidget, AWinControl, AParams);
  {$IFDEF DebugLCLComponents}
  DebugGtkWidgets.MarkCreated(Pointer(AWidget), dbgsName(AWinControl));
  {$ENDIF}
  gtk_notebook_set_scrollable(AWidget, True);

  if not (nboHidePageListPopup in TCustomTabControl(AWinControl).Options) then
    gtk_notebook_popup_enable(AWidget);

  {$IFDEF GTK2USEDUMMYNOTEBOOKPAGE}
  if TCustomTabControl(AWinControl).PageCount=0 then
    // a gtk notebook needs a page -> add dummy page
    Gtk2WidgetSet.AddDummyNoteBookPage(AWidget);
  {$ENDIF}

  gtk_notebook_set_tab_pos(AWidget, GtkPositionTypeMap[TCustomTabControl(AWinControl).TabPosition]);
  Result := HWND(TLCLIntfHandle({%H-}PtrUInt(AWidget)));
  Set_RC_Name(AWinControl, PGtkWidget(AWidget));
  SetCallBacks(PGtkWidget(AWidget), WidgetInfo);
end;

class function TGtk2WSCustomTabControl.GetDefaultClientRect(
  const AWinControl: TWinControl; const aLeft, aTop, aWidth, aHeight: integer;
  var aClientRect: TRect): boolean;
var
  FrameBorders: TRect;
begin
  Result:=false;
  if (AWinControl is TTabControl) then
  begin
    // use normal ClientRect
  end else begin
    // handle is a gtknotebook
    //DebugLn(['TGtk2WSCustomTabControl.GetDefaultClientRect ',DbgSName(AWinControl),' ',aWidth,'x',aHeight]);
    if AWinControl.HandleAllocated
    and (gtk_notebook_get_nth_page({%H-}PGtkNotebook(AWinControl.Handle),0)<>nil)
    then begin
      // notebook handle allocated and has one page
      // => normal GetClientRect will retrieve the right ClientRect
    end else begin
      FrameBorders:=GetStyleNotebookFrameBorders;
      aClientRect:=Rect(0,0,
                   Max(0,aWidth-FrameBorders.Left-FrameBorders.Right),
                   Max(0,aHeight-FrameBorders.Top-FrameBorders.Bottom));
      Result:=true;
      {$IFDEF VerboseSizeMsg}
      DebugLn(['TGtk2WSCustomTabControl.GetDefaultClientRect END FrameBorders=',dbgs(FrameBorders),' aClientRect=',dbgs(aClientRect)]);
      {$ENDIF}
    end;
  end;
end;

class procedure TGtk2WSCustomTabControl.AddPage(const ATabControl: TCustomTabControl;
  const AChild: TCustomPage; const AIndex: integer);
{
  Inserts a new page to a notebook at position Index. The ATabControl is a
  TCustomTabControl, the AChild one of its TCustomPage. Both handles must already
  be created. ATabControl Handle is a PGtkNoteBook and APage handle is a
  PGtkHBox.
  This procedure creates a new tab with an optional image, the page caption and
  an optional close button. The image and the caption will also be added to the
  tab popup menu.
}
var
  NoteBookWidget: PGtkWidget;  // the notebook
  PageWidget: PGtkWidget;      // the page (content widget)
  TabWidget: PGtkWidget;       // the tab (hbox containing a pixmap, a label
                               //          and a close button)
  TabLabelWidget: PGtkWidget;  // the label in the tab
  MenuWidget: PGtkWidget;      // the popup menu (hbox containing a pixmap and
                               // a label)
  MenuLabelWidget: PGtkWidget; // the label in the popup menu item
begin
  {$IFDEF NOTEBOOK_DEBUG}
  DebugLn(['TGtkWSCustomTabControl.AddPage ',dbgsName(ATabControl),' ',ATabControl.HandleAllocated,' AChild=',dbgsName(AChild),' ',AChild.HandleAllocated,' Child.TabVisible=',AChild.TabVisible]);
  {$ENDIF}
  NoteBookWidget := {%H-}PGtkWidget(ATabControl.Handle);
  PageWidget := {%H-}PGtkWidget(AChild.Handle);

  // set LCL size
  AChild.SetBounds(AChild.Left, AChild.Top, ATabControl.ClientWidth, ATabControl.ClientHeight);

  if AChild.TabVisible then
    gtk_widget_show(PageWidget);

  // Check if already created. if so just show it because it is invisible
  if gtk_notebook_get_tab_label(PGtkNoteBook(NoteBookWidget), PageWidget) <> nil
  then begin
    {$IFDEF NOTEBOOK_DEBUG}
    DebugLn(['TGtkWSCustomTabControl.AddPage already added']);
    {$ENDIF}
    exit;
  end;

  // create the tab (hbox container)
  TabWidget := gtk_hbox_new(false, 1);
  g_object_set_data(PGObject(TabWidget), 'TabImage', nil);
  g_object_set_data(PGObject(TabWidget), 'TabCloseBtn', nil);
  // put a label into the tab
  TabLabelWidget := gtk_label_new('');
  g_object_set_data(PGObject(TabWidget), 'TabLabel', TabLabelWidget);
  gtk_widget_show(TabLabelWidget);
  gtk_box_pack_start_defaults(PGtkBox(TabWidget), TabLabelWidget);

  if AChild.TabVisible then
    gtk_widget_show(TabWidget);

  // create popup menu item
  MenuWidget := gtk_hbox_new(false, 2);
  // set icon widget to nil
  g_object_set_data(PGObject(MenuWidget), 'TabImage', nil);
  // put a label into the menu
  MenuLabelWidget := gtk_label_new('');
  g_object_set_data(PGObject(MenuWidget), 'TabMenuLabel', MenuLabelWidget);
  gtk_widget_show(MenuLabelWidget);
  gtk_box_pack_start_defaults(PGtkBox(MenuWidget), MenuLabelWidget);

  if AChild.TabVisible then
    gtk_widget_show(MenuWidget);

  {$IFDEF GTK2USEDUMMYNOTEBOOKPAGE}
  // remove the dummy page (a gtk_notebook needs at least one page)
  RemoveDummyNoteBookPage(PGtkNotebook(NoteBookWidget));
  {$ENDIF}
  // insert the page
  gtk_notebook_insert_page_menu(PGtkNotebook(NotebookWidget), PageWidget,
    TabWidget, MenuWidget, AIndex);

  UpdateNotebookPageTab(ATabControl, AChild);
  UpdateNoteBookClientWidget(ATabControl);
  UpdateNotebookTabFont(AChild, AChild.Font);

  // init the size of the page widget
  //DebugLn(['TGtkWSCustomTabControl.AddPage ',DbgSName(ATabControl),' ',dbgs(ATabControl.BoundsRect)]);
  {$IFDEF VerboseSizeMsg}
  DebugLn(['TGtkWSCustomTabControl.AddPage PageWidget^.allocation=',dbgs(PageWidget^.allocation),' NotebookWidget=',dbgs(NotebookWidget^.allocation)]);
  {$ENDIF}
end;

class procedure TGtk2WSCustomTabControl.MovePage(const ATabControl: TCustomTabControl;
  const AChild: TCustomPage; const NewIndex: integer);
var
  NoteBookWidget: PGtkNotebook;
begin
  NoteBookWidget:={%H-}PGtkNotebook(ATabControl.Handle);
  gtk_notebook_reorder_child(NoteBookWidget, {%H-}PGtkWidget(AChild.Handle), NewIndex);
  UpdateNoteBookClientWidget(ATabControl);
end;

class function TGtk2WSCustomTabControl.GetCapabilities: TCTabControlCapabilities;
begin
  Result:=[nbcPageListPopup, nbcShowCloseButtons];
end;

class function TGtk2WSCustomTabControl.GetNotebookMinTabHeight(
  const AWinControl: TWinControl): integer;
var
  NBWidget: PGTKWidget;
  BorderWidth: Integer;
  Page: PGtkNotebookPage;
begin
  Result:=inherited GetNotebookMinTabHeight(AWinControl);
  //debugln('TGtkWSCustomTabControl.GetNotebookMinTabHeight A ',dbgs(Result));
  exit;

  debugln('TGtkWSCustomTabControl.GetNotebookMinTabHeight A ',dbgs(AWinControl.HandleAllocated));
  if AWinControl.HandleAllocated then
    NBWidget:={%H-}PGTKWidget(AWinControl.Handle)
  else
    NBWidget:=GetStyleWidget(lgsNotebook);

  // ToDo: find out how to create a fully working hidden Notebook style widget

  if (NBWidget=nil) then begin
    Result:=TWSCustomTabControl.GetNotebookMinTabHeight(AWinControl);
    exit;
  end;
  debugln('TGtkWSCustomTabControl.GetNotebookMinTabHeight NBWidget: ',GetWidgetDebugReport(NBWidget),
   ' ',dbgs(NBWidget^.allocation.width),'x',dbgs(NBWidget^.allocation.height));

  BorderWidth:=(PGtkContainer(NBWidget)^.flag0 and bm_TGtkContainer_border_width)
               shr bp_TGtkContainer_border_width;
  if PGtkNoteBook(NBWidget)^.first_tab<>nil then
    Page:=PGtkNoteBook(NBWidget)^.cur_page;

  Result:=BorderWidth;
  if (Page<>nil) then begin
    debugln('TGtkWSCustomTabControl.RemovePage TODO');
  end;
  debugln('TGtkWSCustomTabControl.GetNotebookMinTabHeight END ',dbgs(Result),' ',
    GetWidgetDebugReport(NBWidget));
end;

class function TGtk2WSCustomTabControl.GetNotebookMinTabWidth(
  const AWinControl: TWinControl): integer;
begin
  Result:=TWSCustomTabControl.GetNotebookMinTabWidth(AWinControl);
end;

class function TGtk2WSCustomTabControl.GetTabIndexAtPos(
  const ATabControl: TCustomTabControl; const AClientPos: TPoint): integer;
var
  NoteBookWidget: PGtkNotebook;
  i: integer;
  TabWidget: PGtkWidget;
  PageWidget: PGtkWidget;
  NotebookPos: TPoint;
  Window: PGdkWindow;
  WindowOrg,ClientOrg: TPoint;
  Count: guint;
begin
  Result:=-1;
  NoteBookWidget:={%H-}PGtkNotebook(ATabControl.Handle);
  if (NotebookWidget=nil) then exit;
  //DebugLn(['TGtkWSCustomTabControl.GetTabIndexAtPos ',GetWidgetDebugReport(PGtkWidget(NotebookWidget))]);
  Window := GetControlWindow(NoteBookWidget);
  gdk_window_get_origin(Window,@WindowOrg.X,@WindowOrg.Y);
  ClientOrg:=GetWidgetClientOrigin(PGtkWidget(NotebookWidget));
  NotebookPos.X:= AClientPos.X + (ClientOrg.X-WindowOrg.X);
  NotebookPos.Y:= AClientPos.Y + (ClientOrg.Y-WindowOrg.Y);
  // go through all tabs
  Count:=g_list_length(NoteBookWidget^.Children);
  for i:=0 to Count-1 do
  begin
    PageWidget:=gtk_notebook_get_nth_page(NoteBookWidget,i);
    if PageWidget<>nil then
    begin
      TabWidget:=gtk_notebook_get_tab_label(NoteBookWidget, PageWidget);
      if (TabWidget<>nil) and GTK_WIDGET_MAPPED(TabWidget) then
      begin
        // test if position is in tabwidget
        if (TabWidget^.Allocation.X<=NoteBookPos.X)
        and (TabWidget^.Allocation.Y<=NoteBookPos.Y)
        and (TabWidget^.Allocation.X+TabWidget^.Allocation.Width>NoteBookPos.X)
        and (TabWidget^.Allocation.Y+TabWidget^.Allocation.Height>NoteBookPos.Y)
        then begin
          Result:=i;
          exit;
        end;
      end;
    end;
  end;
end;

class function TGtk2WSCustomTabControl.GetTabRect(const ATabControl: TCustomTabControl;
  const AIndex: Integer): TRect;
var
  NoteBookWidget: PGtkNotebook;
  TabWidget: PGtkWidget;
  PageWidget: PGtkWidget;
  Count: guint;
begin
  Result := inherited;
  NoteBookWidget:={%H-}PGtkNotebook(ATabControl.Handle);
  if (NotebookWidget=nil) then exit;

  Count := g_list_length(NoteBookWidget^.Children);
  PageWidget := gtk_notebook_get_nth_page(NoteBookWidget, AIndex);
  if (PageWidget<>nil) and (AIndex < Count) then
  begin
    TabWidget := gtk_notebook_get_tab_label(NoteBookWidget, PageWidget);
    if TabWidget <> nil then
      Result := RectFromGdkRect(TabWidget^.allocation);
  end;
end;

class procedure TGtk2WSCustomTabControl.SetPageIndex(
  const ATabControl: TCustomTabControl; const AIndex: integer);
var
  GtkNotebook: PGtkNotebook;
  ANewIndex: Integer;
  Page: PGtkWidget;
begin
  if not WSCheckHandleAllocated(ATabControl, 'SetPageIndex') then
    Exit;
  if (AIndex < 0) or (AIndex > ATabControl.PageCount - 1) then
    exit;
  GtkNotebook := {%H-}PGtkNoteBook(ATabControl.Handle);
  if gtk_notebook_get_current_page(GtkNotebook) <> AIndex then
  begin
    // gtk2 cannot set page if some tab in between tabvisible=false, so
    // we must compare page handles.
    if ATabControl.Page[AIndex].HandleAllocated then
    begin
      Page := {%H-}PGtkWidget(ATabControl.Page[AIndex].Handle);
      ANewIndex := gtk_notebook_page_num(GtkNoteBook, Page);
      g_object_set_data(PGObject(GtkNotebook), LCL_NotebookManualPageSwitchKey, ATabControl);
      gtk_notebook_set_page(GtkNotebook, ANewIndex);
    end;
  end;
  UpdateNoteBookClientWidget(ATabControl);
end;

class procedure TGtk2WSCustomTabControl.SetTabPosition(
  const ATabControl: TCustomTabControl; const ATabPosition: TTabPosition);
begin
  gtk_notebook_set_tab_pos({%H-}PGtkNotebook(ATabControl.Handle),
    GtkPositionTypeMap[ATabPosition]);
end;

class procedure TGtk2WSCustomTabControl.ShowTabs(const ATabControl: TCustomTabControl;
  AShowTabs: boolean);
begin
  if IsTTabControl({%H-}PGtkWidget(ATabControl.Handle)) then
    {$IFDEF NOTEBOOK_DEBUG}
    writeln('**** TGtk2WSCustomTabControl.ShowTabs DO NOT SHOW TABS ON CUSTOM CONTROL !')
    {$ENDIF}
  else
    gtk_notebook_set_show_tabs({%H-}PGtkNotebook(ATabControl.Handle), AShowTabs);
end;

class procedure TGtk2WSCustomTabControl.UpdateProperties(const ATabControl: TCustomTabControl);
begin
  if (nboHidePageListPopup in ATabControl.Options) then
    gtk_notebook_popup_disable({%H-}PGtkNotebook(ATabControl.Handle))
  else
    gtk_notebook_popup_enable({%H-}PGtkNotebook(ATabControl.Handle));
end;



{ TGtk2WSCustomPage }

class procedure TGtk2WSCustomPage.SetCallbacks(const AGtkWidget: PGtkWidget;
  const AWidgetInfo: PWidgetInfo);
begin
  TGtk2WSWinControl.SetCallbacks(PGtkObject(AGtkWidget), TComponent(AWidgetInfo^.LCLObject));
end;

class function TGtk2WSCustomPage.CreateHandle(const AWinControl: TWinControl;
  const AParams: TCreateParams): TLCLIntfHandle;
var
  Widget: PGtkWidget;
  WidgetInfo: PWidgetInfo;
begin
  Widget := Gtk2Widgetset.CreateSimpleClientAreaWidget(AWinControl, True);
  {$IFDEF DebugLCLComponents}
  DebugGtkWidgets.MarkCreated(Widget, dbgsName(AWinControl));
  {$ENDIF}
  Result := TLCLIntfHandle({%H-}PtrUInt(Widget));

  WidgetInfo := GetWidgetInfo(Widget);
  WidgetInfo^.LCLObject := AWinControl;
  WidgetInfo^.Style := AParams.Style;
  WidgetInfo^.ExStyle := AParams.ExStyle;
  WidgetInfo^.WndProc := {%H-}PtrUInt(AParams.WindowClass.lpfnWndProc);

  Set_RC_Name(AWinControl, Widget);
  SetCallBacks(Widget, WidgetInfo);
end;

class procedure TGtk2WSCustomPage.UpdateProperties(const ACustomPage: TCustomPage);
var
  NoteBook: PGtkWidget;
  PageWidget: PGtkWidget;
  TabWidget: PGtkWidget;
  TabImageWidget: PGtkWidget;
begin
  UpdateNotebookPageTab(nil, ACustomPage);
  {we must update our icon (if exists) otherwise it will be updated only
  when our tab reach focus}
  if not (csDesigning in ACustomPage.ComponentState)
    and not ACustomPage.TabVisible
    or not ACustomPage.HandleAllocated
    or not Assigned(ACustomPage.Parent)
  then
    exit;

  PageWidget := {%H-}PGtkWidget(ACustomPage.Handle);
  NoteBook := {%H-}PGtkWidget(ACustomPage.Parent.Handle);
  if (NoteBook = nil) or not GTK_IS_NOTEBOOK(NoteBook) then
    exit;

  TabWidget := gtk_notebook_get_tab_label(PGtkNoteBook(Notebook), PageWidget);
  if (TabWidget = nil) or not GTK_WIDGET_VISIBLE(TabWidget) then
    exit;

  TabImageWidget := g_object_get_data(PGObject(TabWidget), 'TabImage');
  if TabImageWidget <> nil then
    gtk_widget_queue_draw(TabImageWidget);
end;

class procedure TGtk2WSCustomPage.SetBounds(const AWinControl: TWinControl;
  const ALeft, ATop, AWidth, AHeight: Integer);
begin
  // ignore resizes from the LCL
end;

class procedure TGtk2WSCustomPage.SetFont(const AWinControl: TWinControl;
  const AFont: TFont);
begin
  if not WSCheckHandleAllocated(AWinControl, 'SetFont') then
    exit;
  UpdateNotebookTabFont(AWinControl, AFont);
end;

class procedure TGtk2WSCustomPage.ShowHide(const AWinControl: TWinControl);
var
  AShow: Boolean;
begin
  if not WSCheckHandleAllocated(AWinControl, 'ShowHide') then
    exit;
  if (csDesigning in AWinControl.ComponentState) then
  begin
    AShow := Assigned(AWinControl.Parent) and (AWinControl.Parent is TCustomTabControl) and
      TCustomTabControl(AWinControl.Parent).ShowTabs;
    TGtk2WidgetSet(WidgetSet).SetVisible(AWinControl,
      AShow);
  end else
    TGtk2WidgetSet(WidgetSet).SetVisible(AWinControl,
      TCustomPage(AWinControl).TabVisible);
end;

class function TGtk2WSCustomPage.GetDefaultClientRect(
  const AWinControl: TWinControl; const aLeft, aTop, aWidth, aHeight: integer;
  var aClientRect: TRect): boolean;
begin
  Result:=false;
  if AWinControl.Parent=nil then exit;
  if AWinControl.HandleAllocated and AWinControl.Parent.HandleAllocated
  and ({%H-}PGtkWidget(AWinControl.Handle)^.parent<>nil) then
  begin

  end else begin
    Result:=true;
    aClientRect:=AWinControl.Parent.ClientRect;
    //DebugLn(['TGtk2WSCustomPage.GetDefaultClientRect ',DbgSName(AWinControl),' Parent=',DbgSName(AWinControl.Parent),' ParentBounds=',dbgs(AWinControl.Parent.BoundsRect),' ParentClient=',dbgs(AWinControl.Parent.ClientRect)]);
  end;
  {$IFDEF VerboseSizeMsg}
  if Result then DebugLn(['TGtk2WSCustomPage.GetDefaultClientRect ',DbgSName(AWinControl),' aClientRect=',dbgs(aClientRect)]);
  {$ENDIF}
end;

