/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/sort.h>
#include <EMotionStudio/EMStudioSDK/Source/EMStudioManager.h>
#include <EMotionStudio/EMStudioSDK/Source/MainWindow.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionEvents/MotionEventPresetsWidget.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionEvents/MotionEventsPlugin.h>
#include <EMotionStudio/Plugins/StandardPlugins/Source/MotionEvents/MotionEventPresetCreateDialog.h>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QFileDialog>
#include <QMenu>
#include <QSettings>
#include <QShortcut>
#include <QKeySequence>
#include <QDragEnterEvent>
#include <QToolBar>


namespace EMStudio
{
    MotionEventPresetsWidget::MotionEventPresetsWidget(QWidget* parent, MotionEventsPlugin* plugin)
        : QWidget(parent)
        , mPlugin(plugin)
    {
        Init();
    }


    void MotionEventPresetsWidget::Init()
    {
        // create the layouts
        QVBoxLayout* layout = new QVBoxLayout();
        QHBoxLayout* ioButtonsLayout = new QHBoxLayout();
        layout->setMargin(0);
        layout->setSpacing(2);

        // create the table widget
        mTableWidget = new DragTableWidget(0, 2, nullptr);
        mTableWidget->setCornerButtonEnabled(false);
        mTableWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        mTableWidget->setContextMenuPolicy(Qt::DefaultContextMenu);
        mTableWidget->setShowGrid(false);

        // set the table to row single selection
        mTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        mTableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

        QHeaderView* horizontalHeader = mTableWidget->horizontalHeader();
        horizontalHeader->setStretchLastSection(true);
        horizontalHeader->setVisible(false);

        QToolBar* toolBar = new QToolBar(this);

        m_addAction = toolBar->addAction(MysticQt::GetMysticQt()->FindIcon("Images/Icons/Plus.svg"),
            tr("Add new motion event preset"),
            this, &MotionEventPresetsWidget::AddMotionEventPreset);

        m_loadAction = toolBar->addAction(MysticQt::GetMysticQt()->FindIcon("Images/Icons/Open.svg"),
            tr("Load motion event preset config file"),
            this, [=]() { LoadPresets(); /* use lambda so that we get the default value for the showDialog parameter */ });

        m_saveMenuAction = toolBar->addAction(
            MysticQt::GetMysticQt()->FindIcon("Images/Icons/Save.svg"),
            tr("Save motion event preset config"));
        {
            QToolButton* toolButton = qobject_cast<QToolButton*>(toolBar->widgetForAction(m_saveMenuAction));
            AZ_Assert(toolButton, "The action widget must be a tool button.");
            toolButton->setPopupMode(QToolButton::InstantPopup);

            QMenu* contextMenu = new QMenu(toolBar);

            m_saveAction = contextMenu->addAction("Save", this, [=]() { SavePresets(); /* use lambda so that we get the default value for the showDialog parameter */ });
            m_saveAsAction = contextMenu->addAction("Save as...", this, &MotionEventPresetsWidget::SaveWithDialog);

            m_saveMenuAction->setMenu(contextMenu);
        }

        layout->addWidget(toolBar);
        layout->addWidget(mTableWidget);
        layout->addLayout(ioButtonsLayout);

        // set the main layout
        setLayout(layout);

        // connect the signals and the slots
        connect(mTableWidget, &MotionEventPresetsWidget::DragTableWidget::itemSelectionChanged, this, &MotionEventPresetsWidget::SelectionChanged);
        connect(mTableWidget, &QTableWidget::cellDoubleClicked, this, [this](int row, int column)
        {
            AZ_UNUSED(column);
            MotionEventPreset* preset = GetEventPresetManager()->GetPreset(row);
            MotionEventPresetCreateDialog createDialog(*preset, this);
            if (createDialog.exec() == QDialog::Accepted)
            {
                *preset = createDialog.GetPreset();

                GetEventPresetManager()->SetDirtyFlag(true);
                ReInit();
                mPlugin->FireColorChangedSignal();
            }
        });


        // initialize everything
        ReInit();
        UpdateInterface();
        mPlugin->ReInit();
    }


    void MotionEventPresetsWidget::ReInit()
    {
        // Remember selected items
        QList<QTableWidgetItem*> selectedItems = mTableWidget->selectedItems();
        AZStd::vector<AZ::u32> selectedRows;
        selectedRows.reserve(selectedItems.size());
        for (const QTableWidgetItem* selectedItem : selectedItems)
        {
            const AZ::u32 row = selectedItem->row();
            selectedRows.emplace_back(row);
        }

        // clear the table widget
        mTableWidget->clear();
        mTableWidget->setColumnCount(2);

        const size_t numEventPresets = GetEventPresetManager()->GetNumPresets();
        mTableWidget->setRowCount(static_cast<int>(numEventPresets));

        // set header items for the table
        QTableWidgetItem* colorHeaderItem = new QTableWidgetItem("Color");
        QTableWidgetItem* presetNameHeaderItem = new QTableWidgetItem("Name");
        colorHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        presetNameHeaderItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        mTableWidget->setHorizontalHeaderItem(0, colorHeaderItem);
        mTableWidget->setHorizontalHeaderItem(1, presetNameHeaderItem);

        mTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        mTableWidget->setColumnWidth(0, 39);

        for (AZ::u32 i = 0; i < numEventPresets; ++i)
        {
            MotionEventPreset* motionEventPreset = GetEventPresetManager()->GetPreset(i);
            if (motionEventPreset == nullptr)
            {
                continue;
            }

            // create table items
            QPixmap colorPixmap(16, 16);
            const QColor eventColor(motionEventPreset->GetEventColor());
            colorPixmap.fill(eventColor);
            QIcon icon;
            icon.addPixmap(colorPixmap);
            icon.addPixmap(colorPixmap, QIcon::Selected);
            QTableWidgetItem* tableItemColor = new QTableWidgetItem(icon, "");
            QTableWidgetItem* tableItemPresetName = new QTableWidgetItem(motionEventPreset->GetName().c_str());

            AZStd::string whatsThisString = AZStd::string::format("%i", i);
            tableItemColor->setWhatsThis(whatsThisString.c_str());
            tableItemPresetName->setWhatsThis(whatsThisString.c_str());

            mTableWidget->setItem(i, 0, tableItemColor);
            mTableWidget->setItem(i, 1, tableItemPresetName);

            // Editing will be handled in the double click signal handler
            tableItemPresetName->setFlags(tableItemPresetName->flags() ^ Qt::ItemIsEditable);

            // Check if row should be selected
            if (AZStd::find(selectedRows.begin(), selectedRows.end(), i) != selectedRows.end())
            {
                tableItemColor->setSelected(true);
                tableItemPresetName->setSelected(true);
            }
        }

        // set the vertical header not visible
        QHeaderView* verticalHeader = mTableWidget->verticalHeader();
        verticalHeader->setVisible(false);

        mTableWidget->resizeColumnToContents(1);
        mTableWidget->resizeColumnToContents(2);

        if (mTableWidget->columnWidth(1) < 36)
        {
            mTableWidget->setColumnWidth(1, 36);
        }

        if (mTableWidget->columnWidth(2) < 70)
        {
            mTableWidget->setColumnWidth(2, 70);
        }

        mTableWidget->horizontalHeader()->setStretchLastSection(true);
        
        // update the interface
        UpdateInterface();
    }


    void MotionEventPresetsWidget::UpdateInterface()
    {
        m_saveAction->setEnabled(!GetEventPresetManager()->GetFileNameString().empty());
    }


    void MotionEventPresetsWidget::AddMotionEventPreset()
    {
        MotionEventPresetCreateDialog createDialog(MotionEventPreset(), this);
        if (createDialog.exec() == QDialog::Rejected)
        {
            return;
        }

        // add the preset
        MotionEventPreset* motionEventPreset = aznew MotionEventPreset(AZStd::move(createDialog.GetPreset()));
        GetEventPresetManager()->AddPreset(motionEventPreset);

        // reinit the dialog
        ReInit();
    }


    void MotionEventPresetsWidget::RemoveMotionEventPreset(uint32 index)
    {
        GetEventPresetManager()->RemovePreset(index);
        ReInit();
    }


    void MotionEventPresetsWidget::RemoveSelectedMotionEventPresets()
    {
        QList<QTableWidgetItem*> selectedItems = mTableWidget->selectedItems();
        if (selectedItems.isEmpty())
        {
            ClearMotionEventPresetsButton();
            return;
        }

        AZStd::vector<int> deleteRows;
        for (const QTableWidgetItem* selectedItem : selectedItems)
        {
            const int row = selectedItem->row();
            if (AZStd::find(deleteRows.begin(), deleteRows.end(), row) == deleteRows.end())
            {
                deleteRows.emplace_back(row);
            }
        }

        const int firstSelectedRow = deleteRows[0];
        AZStd::sort(deleteRows.begin(), deleteRows.end(), AZStd::greater<int>());

        // Remove all selected rows back-to-front.
        for (const int row : deleteRows)
        {
            RemoveMotionEventPreset(row);
        }

        ReInit();

        // selected the next row
        if (firstSelectedRow > (mTableWidget->rowCount() - 1))
        {
            mTableWidget->selectRow(firstSelectedRow - 1);
        }
        else
        {
            mTableWidget->selectRow(firstSelectedRow);
        }
    }


    void MotionEventPresetsWidget::ClearMotionEventPresetsButton()
    {
        // show message box
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Delete All Motion Event Presets?");
        msgBox.setText("Are you sure to really delete all motion event presets?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int result = msgBox.exec();

        // only delete if result was yes
        if (result == QMessageBox::Yes)
        {
            ClearMotionEventPresets();
        }
    }


    void MotionEventPresetsWidget::ClearMotionEventPresets()
    {
        mTableWidget->selectAll();
        RemoveSelectedMotionEventPresets();
        UpdateInterface();
    }


    void MotionEventPresetsWidget::LoadPresets(bool showDialog)
    {
        if (showDialog)
        {
            GetManager()->SetAvoidRendering(true);
            const QString filename = QFileDialog::getOpenFileName(this, "Open", GetEventPresetManager()->GetFileName(), "EMStudio Config Files (*.cfg);;All Files (*)");
            GetManager()->SetAvoidRendering(false);

            if (filename.isEmpty() == false)
            {
                GetEventPresetManager()->Load(FromQtString(filename).c_str());
            }
        }
        else
        {
            GetEventPresetManager()->Load();
        }

        ReInit();
        UpdateInterface();
        mPlugin->ReInit();
    }


    void MotionEventPresetsWidget::SavePresets(bool showSaveDialog)
    {
        if (showSaveDialog)
        {
            GetManager()->SetAvoidRendering(true);

            AZStd::string defaultFolder;
            AzFramework::StringFunc::Path::GetFullPath(GetEventPresetManager()->GetFileName(), defaultFolder);

            const QString filename = QFileDialog::getSaveFileName(this, "Save", defaultFolder.c_str(), "EMotionFX Event Preset Files (*.cfg);;All Files (*)");
            GetManager()->SetAvoidRendering(false);

            if (!filename.isEmpty())
            {
                GetEventPresetManager()->SaveAs(filename.toUtf8().data());
            }
        }
        else
        {
            GetEventPresetManager()->Save();
        }
        UpdateInterface();
    }


    void MotionEventPresetsWidget::SaveWithDialog()
    {
        SavePresets(true);
    }


    void MotionEventPresetsWidget::contextMenuEvent(QContextMenuEvent* event)
    {
        QList<QTableWidgetItem*> selectedItems = mTableWidget->selectedItems();
        if (selectedItems.isEmpty())
        {
            return;
        }

        QMenu menu(this);

        menu.addAction(tr("Remove selected motion event presets"),
            this, &MotionEventPresetsWidget::RemoveSelectedMotionEventPresets);

        if (!menu.isEmpty())
        {
            menu.exec(event->globalPos());
        }
    }


    void MotionEventPresetsWidget::keyPressEvent(QKeyEvent* event)
    {
        // delete key
        if (event->key() == Qt::Key_Delete)
        {
            RemoveSelectedMotionEventPresets();
            event->accept();
            return;
        }

        // base class
        QWidget::keyPressEvent(event);
    }


    void MotionEventPresetsWidget::keyReleaseEvent(QKeyEvent* event)
    {
        // delete key
        if (event->key() == Qt::Key_Delete)
        {
            event->accept();
            return;
        }

        // base class
        QWidget::keyReleaseEvent(event);
        ReInit();
    }
} // namespace EMStudio
