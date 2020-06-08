﻿#include <control/ui_listbox.h>
#include <control/ui_listitem.h>
#include <control/ui_listcol.h>
#include <control/ui_listcols.h>
#include <control/ui_listhead.h>
#include <control/ui_listheader.h>

#include <core/ui_ctrlmeta.h>
#include <control/ui_image.h>
#include <control/ui_label.h>
#include <control/ui_box.h>
#include <input/ui_kminput.h>
#include <constexpr/const_bkdr.h>
#include <debugger/ui_debug.h>

#include <core/ui_unsafe.h>
#include "../private/ui_private_control.h"

#include <algorithm>

// ui namespace
namespace LongUI {
    // UIListBox类 元信息
    LUI_CONTROL_META_INFO(UIListBox, "listbox");


    /*
    1. 每行高度(名词)一致, 选取最大的一个作为基准点
    2. rows属性影响minsize


    <listbox>

      <listcols>
        <listcol/>
        <listcol/>
        <!--splitter/-->
        <listcol/>
      </listcols>

      <listhead>
        <listheader label="Name" />
        <listheader label="Sex" />
        <listheader label="Color" />
      </listhead>

      <listitem>
        <label value="Pearl" />
        <label value="Female" />
        <label value="Gray" />
      </listitem>
      <listitem>
        <label value="Aramis" />
        <label value="Male" />
        <label value="Black" />
      </listitem>
      <listitem>
        <label value="Yakima" />
        <label value="Male" />
        <label value="Holstein" />
      </listitem>
    </listbox>

    */
}


/// <summary>
/// Finalizes an instance of the <see cref="UIListBox"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIListBox::~UIListBox() noexcept {
    // 需要提前释放
    m_state.destructing = true;
}


/// <summary>
/// Initializes a new instance of the <see cref="UIListBox" /> class.
/// </summary>
/// <param name="meta">The meta.</param>
LongUI::UIListBox::UIListBox(const MetaControl& meta) noexcept : Super(meta),
    m_oListboxBody(this) {
    // 焦点允许
    //m_state.focusable = true;
    // 默认为列表框
    m_oStyle.appearance = Appearance_WeakApp | Appearance_ListBox;
    // 默认样式
    m_oBox.border = { 1, 1, 1, 1 };
    m_oBox.margin = { 4, 2, 4, 2 };
    // 至少一个
    m_selected.reserve(1);
    // TODO: OOM处理

    // 创建Body控件
    m_oListboxBody.SetAutoOverflow();
#ifndef NDEBUG
    m_oListboxBody.SetDebugName("listbox::listboxbody");
#endif // !NDEBUG
}


/// <summary>
/// Sets the size of the line.
/// </summary>
/// <param name="size">The size.</param>
/// <returns></returns>
void LongUI::UIListBox::SetLineSize(Size2F size) noexcept {
    if (m_oListboxBody.line_size.height != size.height) {
        m_oListboxBody.line_size = size;
    }
}

/// <summary>
/// Updates this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListBox::Update(UpdateReason reason) noexcept {
    // 其他的交给超类处理
    Super::Update(reason);
    // 要求重新布局
    // XXX: 检查重新布局的情况
    if (reason & Reason_BasicRelayout) this->relayout();
}


/// <summary>
/// Relayouts this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListBox::relayout() noexcept {
    // 获取左上角位置
    auto lt = this->RefBox().GetContentPos();
    auto ctsize = this->RefBox().GetContentSize();
    const auto xofffset = lt.x;

    // 先布局列对象(有面积!)
    if (m_pCols) {
        m_pCols->SetPos(lt);
        this->resize_child(*m_pCols, ctsize);
        // 下次再来
        if (m_pCols->WillRelayout())
            return this->NeedUpdate(Reason_ChildLayoutChanged);
        //int bk = 9;
    }
    // 拥有头对象?
    if (m_pHead) {
        m_pHead->SetPos(lt);
        const auto height = m_pHead->GetMinSize().height;
        lt.y += height;
        ctsize.height -= height;
        this->resize_child(*m_pHead, { ctsize.width, height });
    }
    // Body 位置大小
    m_oListboxBody.SetPos(lt);
    this->resize_child(m_oListboxBody, ctsize);

    // == Item ==
    float item_offset = 0.f;
    // 先获取获取行高度
    const auto line_height = [this]() noexcept {
        float height = 0.f;
        for (auto* child : m_list) 
            height = std::max(height, child->GetMinSize().height);
        return height;
    }();
    // 遍历有效子节点
    for (auto* child : m_list) {
        child->SetPos({xofffset, item_offset });
        this->resize_child(*child, { ctsize.width,  line_height });
        item_offset += line_height;
    }
    // TODO: 宽度
    m_oListboxBody.ForceUpdateScrollSize({ ctsize.width, item_offset });
}


/// <summary>
/// Refreshes the index of the item.
/// </summary>
/// <returns></returns>
void LongUI::UIListBox::refresh_item_index() noexcept {
    uint32_t index = 0;
    for (auto child : m_list) {
        UIControlPrivate::SetParentData(*child, index);
        ++index;
    }
    this->clear_need_refresh_index();
}

/// <summary>
/// Adds the attribute.
/// </summary>
/// <param name="key">The key.</param>
/// <param name="value">The value.</param>
/// <returns></returns>
void LongUI::UIListBox::add_attribute(uint32_t key, U8View value) noexcept {
    constexpr auto BKDR_SELTYPE     = 0xdee1c438_ui32;
    constexpr auto BKDR_ROWS        = 0x0f63dd45_ui32;
    switch (key)
    {
    case BKDR_SELTYPE:
        // seltype      : 选择类型
        m_seltype = AttrParser::Seltype(value);
        break;
    case BKDR_ROWS:
        // rows         : 显示列数
        m_displayRow = value.ToInt32();
        break;
    default:
        // 其他交给超类处理
        return Super::add_attribute(key, value);
    }
}


/// <summary>
/// Adds the child.
/// </summary>
/// <param name="child">The child.</param>
/// <returns></returns>
void LongUI::UIListBox::add_child(UIControl& child) noexcept {
    // 是ListItem
    if (const auto item = uisafe_cast<UIListItem>(&child)) {
        m_list.push_back(item);
        this->mark_need_refresh_index();
        UIControlPrivate::AddChild(m_oListboxBody, child);
        return;
    }
    // 是ListCols
    else if (uisafe_cast<UIListCols>(&child)) {
        m_pCols = static_cast<UIListCols*>(&child);
        Super::add_child(child);
        // 交换body cols以提高优先级处理鼠标消息
        this->SwapNode(child, m_oListboxBody);
        // 正在添加节点, 没必要用SwapChildren?
        //this->SwapChildren(child, *m_pListboxBody);
        return;
    }
    // 是ListHead
    else if (uisafe_cast<UIListHead>(&child)) {
        m_pHead = static_cast<UIListHead*>(&child);
    }
    // 调用超类接口
    Super::add_child(child);
}


/// <summary>
/// Inserts the item.
/// </summary>
/// <param name="index">The index.</param>
/// <param name="">The .</param>
/// <returns></returns>
auto LongUI::UIListBox::InsertItem(uint32_t index, const CUIString &) noexcept -> UIListItem* {
    assert(!"NOT IMPL");
    return nullptr;
}


/// <summary>
/// Clears the select.
/// </summary>
/// <returns></returns>
void LongUI::UIListBox::ClearAllSelected() noexcept {
    for (auto* item : m_selected) {
        item->StartAnimation({ State_Selected, State_Non });
    }
    m_selected.clear();
}

/// <summary>
/// Clears the selected.
/// </summary>
/// <param name="item">The item.</param>
/// <returns></returns>
void LongUI::UIListBox::ClearSelected(UIListItem& item) noexcept {
    const auto enditr = m_selected.end();
    const auto itr = std::find(m_selected.begin(), enditr, &item);
    m_selected.erase(itr);
    item.StartAnimation({ State_Selected, State_Non });
}

/// <summary>
/// Selects the item.
/// </summary>
/// <param name="item">The item.</param>
/// <param name="add">if set to <c>true</c> [add].</param>
/// <returns></returns>
void LongUI::UIListBox::SelectItem(UIListItem& item, bool exadd) noexcept {
    // 在表内额外添加就算了
    if (item.IsSelected() && exadd) return;
    // 记录上次操作对象
    m_pLastOp = &item;
    // 清空?
    if (!exadd || !this->IsMultiple()) this->ClearAllSelected();
    // 写入表内
    this->select_item(item);
}

PCN_NOINLINE
/// <summary>
/// Selects the item.
/// </summary>
/// <param name="item">The item.</param>
/// <returns></returns>
void LongUI::UIListBox::select_item(UIListItem& item) noexcept {
    // 写入表内
    // TODO: 链表实现m_selected
    m_selected.push_back(&item);
    item.StartAnimation({ State_Selected, State_Selected });
}


/// <summary>
/// Selects to.
/// </summary>
/// <param name="item">The item.</param>
/// <returns></returns>
void LongUI::UIListBox::SelectTo(UIListItem& item) noexcept {
    assert(!m_list.empty() && "WTF?!");
    // 单选?
    if (!this->IsMultiple()) return this->SelectItem(item, false);
    // 起点
    const auto start = m_pLastOp ? m_pLastOp : m_list[0];
    const auto beginitr = m_list.begin();
    const auto enditr = m_list.end();
    // XXX: 直接暴力搜索(考虑使用m_uData4Parent?)
    const auto startitr = std::find(beginitr, enditr, start);
    const auto itemitr = std::find(beginitr, enditr, &item);
    assert(startitr != enditr);
    assert(itemitr != enditr);
    // 清楚
    this->ClearAllSelected();
    // 开始遍历
    const auto selbegin = startitr < itemitr ? startitr : itemitr;
    const auto selend = (startitr < itemitr ? itemitr : startitr) + 1;
    for (auto itr = selbegin; itr != selend; ++itr) {
        const auto ctrl = *itr;
        this->select_item(*ctrl);
    }
}

PCN_NOINLINE
/// <summary>
/// Gets the index of the item.
/// </summary>
/// <param name="item">The item.</param>
/// <returns></returns>
auto LongUI::UIListBox::GetItemIndex(const UIListItem& item) noexcept -> uint32_t {
    if (this->need_refresh_index()) this->refresh_item_index();
    return UIControlPrivate::GetParentData(item);
}

/// <summary>
/// Items the removed.
/// </summary>
/// <param name="item">The item.</param>
/// <returns></returns>
void LongUI::UIListBox::ItemRemoved(UIListItem& item) noexcept {
    // 删除中直接返回?
    if (m_state.destructing) return;
    // 0. 删除上次操作对象
    if (m_pLastOp == &item) m_pLastOp = nullptr;
    // 1. 删除在表数据
    {
        const auto enditr = m_list.end();
        const auto itr = std::find(m_list.begin(), enditr, &item);
        // !!必须在
        assert(itr != enditr);
        m_list.erase(itr);
    }
    // 2. 删除在选数据
    {
        const auto enditr = m_selected.end();
        const auto itr = std::find(m_selected.begin(), enditr, &item);
#ifndef NDEBUG
        // 自己写的允许擦除end迭代器
        m_selected.erase(m_selected.end());
#endif
        //if (itr != enditr)
        m_selected.erase(itr);
    }
    // 3. 标记需要修改索引
    this->mark_need_refresh_index();
}

/// <summary>
/// Does the event.
/// </summary>
/// <param name="sender">The sender.</param>
/// <param name="e">The e.</param>
/// <returns></returns>
auto LongUI::UIListBox::DoEvent(UIControl* sender, 
    const EventArg& e) noexcept -> EventAccept {
    switch (e.nevent)
    {
    case NoticeEvent::Event_RefreshBoxMinSize:
        // 先更新cols的最小大小以确定宽度
        this->refresh_cols_minsize();
        // 更新最小大小
        //Super::DoEvent(sender, e);
        this->refresh_minsize();
        return Event_Accept;
    default:
        // 其他事件
        return Super::DoEvent(sender, e);
    }
}

/// <summary>
/// Refreshes the minsize.
/// </summary>
/// <returns></returns>
void LongUI::UIListBox::refresh_cols_minsize() noexcept {
    // 根本没有列信息
    if (!m_pCols) return;
    // 内存不足?
    m_minwidth.resize(m_pCols->GetChildrenCount());
    if (!m_minwidth.is_ok()) return;
    // 清空
    std::memset(m_minwidth.data(), 0, m_pCols->GetChildrenCount() * sizeof(float));
    const auto end = m_minwidth.end();
    // 遍历数据
    for (auto ctrl : m_list) {
        auto itr = m_minwidth.begin();
        for (auto& child : (*ctrl)) {
            if (itr == end) break;
            const auto ms = child.GetMinSize();
            float& minwidth = *itr;
            minwidth = std::max(minwidth, ms.width);
            ++itr;
        }
    }
    // 设置
    auto itr = m_minwidth.begin();
    for (auto& child : (*m_pCols)) {
        UIControlPrivate::SetBoxMinWidth(child, *itr);
        ++itr;
    }
    m_pCols->DoEvent(this, { NoticeEvent::Event_RefreshBoxMinSize });
}


/// <summary>
/// Refreshes the minsize.
/// </summary>
/// <returns></returns>
void LongUI::UIListBox::refresh_minsize() noexcept {
    Size2F msize = { 0.f };
    // 先确定Head
    if (m_pHead) msize = m_pHead->RefBox().minsize;
    // 再确定Body
    const auto line_height = m_oListboxBody.line_size.height;
    msize.height += m_displayRow * line_height;
    // 设置大小
    this->set_contect_minsize(msize);
}

// ----------------------------------------------------------------------------
// --------------------           List Item           -------------------------
// ----------------------------------------------------------------------------

// ui namespace
namespace LongUI {
    // UIListItem类 元信息
    LUI_CONTROL_META_INFO(UIListItem, "listitem");
    // UIListItem类 私有实现
    struct UIListItem::Private {
        // 设置新的字符串
        template<typename T> static auto SetText(UIListItem& obj, T && text) noexcept {
            obj.add_private_child();
            if (obj.m_oLabel.SetText(std::forward<T>(text))) {
#ifdef LUI_ACCESSIBLE
                // TODO: ACCESSIBLE
                //LongUI::Accessible(m_pAccessible, Callback_PropertyChanged);
#endif
            }
        }
    };
}

/// <summary>
/// Gets the text.
/// </summary>
/// <returns></returns>
auto LongUI::UIListItem::GetText() const noexcept -> const char16_t* {
    return m_oLabel.GetText();
}


/// <summary>
/// set checked
/// </summary>
/// <param name="checked"></param>
/// <returns></returns>
void LongUI::UIListItem::SetChecked(bool checked) noexcept {
    // 禁用状态
    if (this->IsDisabled()) return;
    if (!!this->IsChecked() == checked) return;
    // TODO: 第三状态?
    //if (this->IsIndeterminate()) 
    const auto state = (m_oStyle.state & State_Checked) ;
    this->StartAnimation({ State_Checked , state ^ State_Checked });
}

/// <summary>
/// Gets the index.
/// </summary>
/// <returns></returns>
auto LongUI::UIListItem::GetIndex() const noexcept -> uint32_t {
    if (const auto list = m_pListBox) {
        return list->GetItemIndex(*this);
    }
    return 0;
}

/// <summary>
/// Gets the text string.
/// </summary>
/// <returns></returns>
auto LongUI::UIListItem::RefText() const noexcept -> const CUIString&{
    return m_oLabel.RefText();
}


/// <summary>
/// Initializes a new instance of the <see cref="UIListItem" /> class.
/// </summary>
/// <param name="meta">The meta.</param>
LongUI::UIListItem::UIListItem(const MetaControl& meta) noexcept : Super(meta),
    m_oImage(nullptr), m_oLabel(nullptr) {
    //m_state.focusable = true;
    m_oBox.padding = { 2, 0, 2, 0 };
    // 阻隔鼠标事件
    m_state.mouse_continue = false;
    this->make_offset_tf_direct(m_oLabel);
    // 水平布局
    m_state.orient = Orient_Horizontal;
    m_oStyle.align = AttributeAlign::Align_Center;
    // 列表项目
    m_oStyle.appearance = Appearance_WeakApp | Appearance_ListItem;
    // 私有实现
    //UIControlPrivate::SetFocusable(image, false);
    //UIControlPrivate::SetFocusable(label, false);
#ifndef NDEBUG
    m_oImage.name_dbg = "listitem::image";
    m_oLabel.name_dbg = "listitem::label";
    assert(m_oImage.IsFocusable() == false);
    assert(m_oLabel.IsFocusable() == false);
#endif
}


/// <summary>
/// Finalizes an instance of the <see cref="UIListItem"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIListItem::~UIListItem() noexcept {
    // 存在提前释放子控件, 需要标记"在析构中"
    m_state.destructing = true;
    // 这里不用输出调试信息
#ifndef NDEBUG
    if (m_pParent)
#endif
    // 操作上级标记自己被删除
    if (const auto list = m_pListBox) {
        list->ItemRemoved(*this);
    }
}

/// <summary>
/// Adds the private child.
/// </summary>
/// <returns></returns>
void LongUI::UIListItem::add_private_child() noexcept {
    if (!m_oImage.GetParent()) {
        this->add_child(m_oImage);
        this->add_child(m_oLabel);
        //m_state.atomicity = true;
    }
}


/// <summary>
/// Relayouts this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListItem::relayout() noexcept {
    // 检查是不是使用的默认控件
    auto& maybe = m_oImage;
    // 根据列表的布局
    if (maybe.GetParent() != this) {
        // 获取UIListBox
        if (const auto list = m_pListBox) {
            if (const auto cols = list->GetCols()) {
                return cols->MatchLayout(*this);
            }
            // TODO: 没有cols的场合?
        }
    }
    // 默认的水平布局
    else this->relayout_h();
}


/// <summary>
/// Sets the text.
/// </summary>
/// <param name="text">The text.</param>
/// <returns></returns>
void LongUI::UIListItem::SetText(const CUIString& text) noexcept {
    return this->SetText(text.view());
}

/// <summary>
/// Sets the text.
/// </summary>
/// <param name="text">The text.</param>
/// <returns></returns>
void LongUI::UIListItem::SetText(U16View text) noexcept {
    Private::SetText(*this, text);
}

/// <summary>
/// Sets the text.
/// </summary>
/// <param name="text">The text.</param>
/// <returns></returns>
void LongUI::UIListItem::SetText(CUIString&& text) noexcept {
    Private::SetText(*this, std::move(text));
}



/// <summary>
/// Does the mouse event.
/// </summary>
/// <param name="e">The e.</param>
/// <returns></returns>
auto LongUI::UIListItem::DoMouseEvent(const MouseEventArg& e) noexcept -> EventAccept {
    switch (e.type)
    {
    case LongUI::MouseEvent::Event_LButtonDown:
        // 复选框
        if (m_type == BehaviorType::Type_Checkbox) {
            this->SetChecked(!this->IsChecked());
        }
        // 存在ListBox
        if (const auto list = m_pListBox) {
            // 判断SHIFT
            if (CUIInputKM::GetKeyState(CUIInputKM::KB_SHIFT)) {
                list->SelectTo(*this);
            }
            else {
                // 判断CTRL键
                const auto ctrl = CUIInputKM::GetKeyState(CUIInputKM::KB_CONTROL);
                if (ctrl && list->IsMultiple() && this->IsSelected()) {
                    // 取消选择
                    list->ClearSelected(*this);
                }
                // 添加选择
                else list->SelectItem(*this, ctrl);
            }
        }
        [[fallthrough]];
    default:
        return Super::DoMouseEvent(e);
    }
}

/// <summary>
/// Updates this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListItem::Update(UpdateReason reason) noexcept {
    // 将文本消息传递给Label
    if (const auto r = reason & Reason_TextFontChanged)
        m_oLabel.Update(r);
    // 获取listbox
    if (reason & Reason_ParentChanged) {
        UIListBox* listbox = nullptr;
        if (m_pParent) {
            const auto ppp = m_pParent->GetParent();
            listbox = longui_cast<UIListBox*>(ppp);
            assert(listbox && "parent but listbox");
            if (m_bSelInit) listbox->SelectItem(*this, false);
        }
        m_pListBox = listbox;
    }
    // TODO: 布局
    if (reason & Reason_BasicRelayout) {
        this->relayout();
#if 1
        // XXX: 直接调用最基的
        UIControl::Update(reason);
#else
        // 取消布局
        Super::Update(reason & ~Reason_BasicRelayout);
#endif
        return;
    }
    // 超类处理
    Super::Update(reason);
}


/// <summary>
/// Does the event.
/// </summary>
/// <param name="sender">The sender.</param>
/// <param name="e">The e.</param>
/// <returns></returns>
auto LongUI::UIListItem::DoEvent(UIControl * sender,
    const EventArg & e) noexcept -> EventAccept {
    switch (e.nevent)
    {
    case NoticeEvent::Event_RefreshBoxMinSize:
        Super::DoEvent(sender, e);
        if (const auto list = m_pListBox)
            list->SetLineSize(this->RefBox().minsize);
        return Event_Accept;
    case NoticeEvent::Event_Initialize:
        // 没子控件
        if (!this->GetChildrenCount()) {
            // TODO: 没有文本时候的处理
            m_oLabel.SetAsDefaultMinsize();
            this->add_private_child();
        }
        this->init_behavior();
        break;
    }
    // 基类处理
    return Super::DoEvent(sender, e);
}

/// <summary>
/// init with behavior
/// </summary>
/// <returns></returns>
void LongUI::UIListItem::init_behavior() noexcept {
    // UIListItem支持checkedbox
    if (m_type == BehaviorType::Type_Checkbox) {
        const auto app = Appearance_CheckBox;
        UIControlPrivate::SetAppearance(m_oImage, app);
        UIControlPrivate::RefStyleState(m_oImage) = m_oStyle.state;
    }
}


/// <summary>
/// Adds the attribute.
/// </summary>
/// <param name="key">The key.</param>
/// <param name="value">The value.</param>
/// <returns></returns>
void LongUI::UIListItem::add_attribute(uint32_t key, U8View value) noexcept {
    // 新增属性列表
    constexpr auto BKDR_TYPE        = 0x0fab1332_ui32;
    constexpr auto BKDR_VALUE       = 0x246df521_ui32;
    constexpr auto BKDR_SELECTED    = 0x03481b1f_ui32;
    // 分类讨论
    switch (key)
    {
    case "label"_bkdr:
        // 传递给子控件
        Unsafe::AddAttrUninited(m_oLabel, BKDR_VALUE, value);
        break;
    case BKDR_TYPE:
        // type : 行为类型
        //      : normal
        //      : checkbox
        m_type = LongUI::ParseBehaviorType(value);
        break;
    case BKDR_SELECTED:
        // 初始化状态
        m_bSelInit = value.ToBool();
        break;
    default:
        // 其他情况, 交给基类处理
        return Super::add_attribute(key, value);
    }
}

// ----------------------------------------------------------------------------
// --------------------           List Cols           -------------------------
// ----------------------------------------------------------------------------

// ui namespace
namespace LongUI {
    // UIListCol类 元信息
    LUI_CONTROL_META_INFO(UIListCols, "listcols");
}

/// <summary>
/// Initializes a new instance of the <see cref="UIListItem" /> class.
/// </summary>
/// <param name="meta">The meta.</param>
LongUI::UIListCols::UIListCols(const MetaControl& meta) noexcept : Super(meta) {
    // 水平布局
    m_state.orient = Orient_Horizontal;
}

/// <summary>
/// Finalizes an instance of the <see cref="UIListCols"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIListCols::~UIListCols() noexcept {
    // TODO: 移除box节点cols对象
}

/// <summary>
/// Adds the child.
/// </summary>
/// <param name="child">The child.</param>
/// <returns></returns>
//void LongUI::UIListCols::add_child(UIControl& child) noexcept {
//    // 是列对象?
//    if (uisafe_cast<UIListCol>(&child)) {
//        m_list.push_back(static_cast<UIListCol*>(&child));
//    }
//    Super::add_child(child);
//}

/// <summary>
/// will relayout
/// </summary>
/// <param name="child">The child.</param>
/// <returns></returns>
bool LongUI::UIListCols::WillRelayout() const noexcept {
    // XXX: 理由
    return !!(m_state.reason & Reason_BasicRelayout);
}


/// <summary>
/// Matches the layout.
/// </summary>
/// <param name="container">The container.</param>
/// <returns></returns>
void LongUI::UIListCols::MatchLayout(UIControl& container) noexcept {
    const auto height = container.RefBox().GetContentSize().height;
    auto itr = container.begin();
    const auto end = container.end();
    // 遍历cols
    for (auto& col : (*this)) {
        // 必须是col
        if (!uisafe_cast<UIListCol>(&col)) continue;
        // 下面没了
        if (itr == end) break;
        // 设置
        auto& cchild = *itr;
        const auto width = col.GetSize().width;
        UIControl::resize_child(cchild, { width, height });
        cchild.SetPos(col.GetPos());
        //LUIDebug(Hint) << (void*)&cchild << cchild.GetPos() << endl;
        // 递进
        ++itr;
    }
}


// ----------------------------------------------------------------------------
// --------------------           List Col            -------------------------
// ----------------------------------------------------------------------------


// ui namespace
namespace LongUI {
    // UIListCol类 元信息
    LUI_CONTROL_META_INFO(UIListCol, "listcol");
}


/// <summary>
/// Finalizes an instance of the <see cref="UIListCol"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIListCol::~UIListCol() noexcept {
}


/// <summary>
/// Initializes a new instance of the <see cref="UIListCol" /> class.
/// </summary>
/// <param name="meta">The meta.</param>
LongUI::UIListCol::UIListCol(const MetaControl& meta) noexcept : Super(meta) {
    // 构造锁
    //impl::ctor_unlock();
}

/// <summary>
/// Renders this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListCol::Render() const noexcept {

}



// ----------------------------------------------------------------------------
// --------------------          List Head            -------------------------
// ----------------------------------------------------------------------------

// ui namespace
namespace LongUI {
    // UIListHead类 元信息
    LUI_CONTROL_META_INFO(UIListHead, "listhead");
}

/// <summary>
/// Initializes a new instance of the <see cref="UIListItem" /> class.
/// </summary>
/// <param name="meta">The meta.</param>
LongUI::UIListHead::UIListHead(const MetaControl& meta) noexcept : Super(meta) {
    // 水平布局
    m_state.orient = Orient_Horizontal;
    // 构造锁
    //impl::ctor_unlock();
}


/// <summary>
/// Finalizes an instance of the <see cref="UIListHead"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIListHead::~UIListHead() noexcept {
    // TODO: 移除box节点head对象

}


/// <summary>
/// Relayouts this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListHead::Update(UpdateReason reason) noexcept {
    // 布局
    if (reason & Reason_BasicRelayout) {
        this->relayout();
        // 略过布局
        return UIControl::Update(reason);
    }
    // 超类处理
    return Super::Update(reason);
}

/// <summary>
/// Relayouts this instance.
/// </summary>
/// <returns></returns>
void LongUI::UIListHead::relayout() noexcept {
    // 获取UIListBox
    if (const auto list = longui_cast<UIListBox*>(m_pParent)) {
        if (const auto cols = list->GetCols()) {
            return cols->MatchLayout(*this);
        }
    }
    // 默认的水平布局
    //this->relayout_h();
}


// ----------------------------------------------------------------------------
// --------------------         List Header           -------------------------
// ----------------------------------------------------------------------------

// ui namespace
namespace LongUI {
    // UIListHeader类 元信息
    LUI_CONTROL_META_INFO(UIListHeader, "listheader");
    // UILabel类 私有实现
    struct UIListHeader::Private {
        // 设置新的字符串
        template<typename T> static auto SetText(UIListHeader& obj, T && text) noexcept {
            if (obj.m_oLabel.SetText(std::forward<T>(text))) {
#ifdef LUI_ACCESSIBLE
                // TODO: ACCESSIBLE
                //LongUI::Accessible(m_pAccessible, Callback_PropertyChanged);
#endif
            }
        }
    };
}



/// <summary>
/// Initializes a new instance of the <see cref="UIListItem" /> class.
/// </summary>
/// <param name="meta">The meta.</param>
LongUI::UIListHeader::UIListHeader(const MetaControl& meta) noexcept : Super(meta),
    m_oImage(this), m_oLabel(this), m_oSortDir(this) {
    //m_state.focusable = true;
    // 内间距
    m_oBox.padding = { 4, 0, 4, 0 };
    // 水平布局
    m_state.orient = Orient_Horizontal;
    // 属于HEADER CELL
    m_oStyle.appearance = Appearance_WeakApp | Appearance_TreeHeaderCell;
    // 私有实现
    //UIControlPrivate::SetFocusable(image, false);
    //UIControlPrivate::SetFocusable(label, false);
#ifndef NDEBUG
    m_oImage.name_dbg = "listheader::image";
    m_oLabel.name_dbg = "listheader::label";
    m_oSortDir.name_dbg = "listheader::sortdir";
    assert(m_oImage.IsFocusable() == false);
    assert(m_oLabel.IsFocusable() == false);
    assert(m_oSortDir.IsFocusable() == false);
#endif
}


/// <summary>
/// Finalizes an instance of the <see cref="UIListItem"/> class.
/// </summary>
/// <returns></returns>
LongUI::UIListHeader::~UIListHeader() noexcept {
    // 存在提前释放子控件, 需要标记"在析构中"
    m_state.destructing = true;
}

/// <summary>
/// 
/// </summary>
/// <param name="child"></param>
/// <returns></returns>
//void LongUI::UIListHeader::add_child(UIControl& child) noexcept {
//    return Super::add_child(child);
//}

/// <summary>
/// Adds the attribute.
/// </summary>
/// <param name="key">The key.</param>
/// <param name="value">The value.</param>
/// <returns></returns>
void LongUI::UIListHeader::add_attribute(uint32_t key, U8View value) noexcept {
    // 新增属性列表
    constexpr auto BKDR_VALUE = 0x246df521_ui32;
    // 分类讨论
    switch (key)
    {
    case "label"_bkdr:
        // 传递给子控件
        Unsafe::AddAttrUninited(m_oLabel, BKDR_VALUE, value);
        break;
    default:
        // 其他情况, 交给基类处理
        return Super::add_attribute(key, value);
    }
}


/// <summary>
/// Sets the text.
/// </summary>
/// <param name="text">The text.</param>
/// <returns></returns>
void LongUI::UIListHeader::SetText(CUIString&& text) noexcept {
    Private::SetText(*this, std::move(text));
}

/// <summary>
/// Sets the text.
/// </summary>
/// <param name="text">The text.</param>
/// <returns></returns>
void LongUI::UIListHeader::SetText(const CUIString & text) noexcept {
    return this->SetText(text.view());
}

/// <summary>
/// Sets the text.
/// </summary>
/// <param name="text">The text.</param>
/// <returns></returns>
void LongUI::UIListHeader::SetText(U16View text) noexcept {
    return Private::SetText(*this, text);
}
