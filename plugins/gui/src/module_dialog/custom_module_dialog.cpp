#include "gui/module_dialog/custom_module_dialog.h"
#include "gui/module_dialog/module_select_model.h"
#include "gui/gui_globals.h"
#include "gui/module_model/module_model.h"
#include "gui/module_model/module_proxy_model.h"
#include "gui/module_model/module_item.h"
#include "gui/searchbar/searchbar.h"
#include "gui/content_manager/content_manager.h"
#include "gui/settings/settings_items/settings_item_keybind.h"
#include "hal_core/utilities/log.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QTreeView>

namespace hal {
    CustomModuleDialog::CustomModuleDialog(QWidget* parent)
        : QDialog(parent),
          mSelectedId(0),
          mLastUsed(nullptr),
          mSearchbar(new Searchbar(this)),
          mNewModule(false)
    {
        setWindowTitle("Move to module …");
        QGridLayout* layout = new QGridLayout(this);

        QPushButton* butNew = new QPushButton("Create new module", this);
        connect(butNew, &QPushButton::pressed, this, &CustomModuleDialog::handleCreateNewModule);
        layout->addWidget(butNew, 0, 0);

        mButtonPick = new QPushButton("Pick module from graph", this);
        connect(mButtonPick, &QPushButton::pressed, this, &CustomModuleDialog::handlePickFromGraph);
        layout->addWidget(mButtonPick, 0, 1);

        QPushButton* butSearch = new QPushButton("Search", this);
        connect(butSearch, &QPushButton::pressed, this, &CustomModuleDialog::handleToggleSearchbar);
        layout->addWidget(butSearch, 0, 2);

        layout->addWidget(mSearchbar, 1, 0, 1, 3);
        mTabWidget = new QTabWidget(this);
        mTreeView  = new QTreeView(mTabWidget);
        mTabWidget->addTab(mTreeView, "Module tree");

        mTableView = new ModuleSelectView(false, mSearchbar, mTabWidget);
        connect(mTableView, &ModuleSelectView::moduleSelected, this, &CustomModuleDialog::handleTableSelection);
        mTabWidget->addTab(mTableView, "Module list");

        if (!ModuleSelectHistory::instance()->isEmpty())
        {
            mLastUsed = new ModuleSelectView(true,mSearchbar,mTabWidget);
            if (mLastUsed->model()->rowCount())
            {
                connect(mLastUsed, &ModuleSelectView::moduleSelected, this, &CustomModuleDialog::handleTableSelection);
                mTabWidget->addTab(mLastUsed, "Recent selection");
            }
            else
            {
                delete mLastUsed;
                mLastUsed = nullptr;
            }

        }

        layout->addWidget(mTabWidget, 2, 0, 1, 3);

        mModuleTreeProxyModel = new ModuleProxyModel(this);
        mModuleTreeProxyModel->setFilterKeyColumn(-1);
        mModuleTreeProxyModel->setDynamicSortFilter(true);
        mModuleTreeProxyModel->setSourceModel(gNetlistRelay->getModuleModel());
        mTreeView->setModel(mModuleTreeProxyModel);
        mTreeView->expandAll();

        mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, this);
        layout->addWidget(mButtonBox, 3, 0, 1, 3, Qt::AlignHCenter);

        mToggleSearchbar = new QAction(this);
        mToggleSearchbar->setShortcut(QKeySequence(ContentManager::sSettingSearch->value().toString()));
        addAction(mToggleSearchbar);

        mTabWidget->setCurrentIndex(1);
        enableButtons();
        mSearchbar->hide();


        connect(mTabWidget, &QTabWidget::currentChanged, this, &CustomModuleDialog::handleCurrentTabChanged);
        connect(mToggleSearchbar, &QAction::triggered, this, &CustomModuleDialog::handleToggleSearchbar);
        connect(mSearchbar, &Searchbar::textEdited, this, &CustomModuleDialog::filter);
        connect(ContentManager::sSettingSearch, &SettingsItemKeybind::keySequenceChanged, this, &CustomModuleDialog::keybindToggleSearchbar);
        connect(mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(mTreeView, &QTreeView::doubleClicked, this, &CustomModuleDialog::handleTreeDoubleClick);
        connect(mTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CustomModuleDialog::handleTreeSelectionChanged);
    }

    void CustomModuleDialog::enableButtons()
    {
        mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(mSelectedId>0);
        mButtonPick->setEnabled(mTableView->model()->rowCount()>0);
        QString target = "…";
        if (mSelectedId>0)
        {
            Module* m = gNetlist->get_module_by_id(mSelectedId);
            if (m) target = QString("%1[%2]").arg(QString::fromStdString(m->get_name())).arg(mSelectedId);
        }
        setWindowTitle("Move to module " + target);
    }

     u32 CustomModuleDialog::treeModuleId(const QModelIndex& index)
     {
        ModuleModel* treeModel = static_cast<ModuleModel*>(mModuleTreeProxyModel->sourceModel());
        Q_ASSERT(treeModel);
        QModelIndex sourceIndex = mModuleTreeProxyModel->mapToSource(index);
        if (!sourceIndex.isValid()) return 0;
        ModuleItem* item = treeModel->getItem(sourceIndex);
        if (!item) return 0;
        return item->id();
     }

     void CustomModuleDialog::handleTreeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
     {
        Q_UNUSED(deselected);
        auto sel = selected.indexes();
        if (sel.empty())
            handleTableSelection(0, false);
        else
            handleTableSelection(treeModuleId(sel.at(0)), false);
     }

     void CustomModuleDialog::handleTreeDoubleClick(const QModelIndex& index)
     {
        u32 moduleId = treeModuleId(index);
        handleTableSelection(moduleId, true);
     }

    void CustomModuleDialog::handleTableSelection(u32 id, bool doubleClick)
    {
        mSelectedId = mSelectExclude.isAccepted(id) ? id : 0;
        enableButtons();
        if (mSelectedId && doubleClick) accept();
    }

    void CustomModuleDialog::handlePickFromGraph()
    {
        ModuleSelectPicker* msp = new ModuleSelectPicker;
        connect(gSelectionRelay, &SelectionRelay::selectionChanged, msp, &ModuleSelectPicker::handleSelectionChanged);
        reject(); // wait for picker, no selection done in dialog
    }

    void CustomModuleDialog::accept()
    {
        ModuleSelectHistory::instance()->add(mSelectedId);
        QDialog::accept();
    }

    void CustomModuleDialog::handleCreateNewModule()
    {
        mNewModule = true;
        QDialog::accept();
    }

    void CustomModuleDialog::handleToggleSearchbar()
    {
        if (mSearchbar->isHidden())
        {
            mSearchbar->show();
            mSearchbar->setFocus();
        }
        else
        {
            mSearchbar->hide();
            setFocus();
        }
    }

    void CustomModuleDialog::handleCurrentTabChanged(int index)
    {
        Q_UNUSED(index);
        mTreeView->clearSelection();
        mTableView->clearSelection();
        mSearchbar->clear();
        if (mLastUsed) mLastUsed->clearSelection();
    }

    void CustomModuleDialog::keybindToggleSearchbar(const QKeySequence& seq)
    {
        mToggleSearchbar->setShortcut(seq);
    }

    void CustomModuleDialog::filter(const QString& text)
    {
        mModuleTreeProxyModel->setFilterRegularExpression(text);
        static_cast<ModuleSelectProxy*>(mTableView->model())->setFilterRegularExpression(text);
        if (mLastUsed)
            static_cast<ModuleSelectProxy*>(mLastUsed->model())->setFilterRegularExpression(text);
        QString output = "navigation regular expression '" + text + "' entered.";
        log_info("user", output.toStdString());
    }
}

