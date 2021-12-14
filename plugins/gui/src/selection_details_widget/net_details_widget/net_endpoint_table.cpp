#include "gui/selection_details_widget/net_details_widget/net_endpoint_table.h"
#include "gui/selection_details_widget/net_details_widget/endpoint_table_model.h"
#include "gui/gui_globals.h"
#include "hal_core/netlist/gate.h"
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <QMenu>
#include "gui/python/py_code_provider.h"
#include <QDebug>

#include "gui/gui_globals.h"

namespace hal
{

    NetEndpointTable::NetEndpointTable(EndpointTableModel* model, QWidget* parent) : QTableView(parent), mEndpointModel(model)
    {
        setContextMenuPolicy(Qt::CustomContextMenu);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSelectionMode(QAbstractItemView::NoSelection);
        setFocusPolicy(Qt::NoFocus);
        horizontalHeader()->setStretchLastSection(true);
        verticalHeader()->hide();
        setModel(mEndpointModel);

        setSelectionBehavior(QAbstractItemView::SelectRows);

        //connections
        connect(this, &QTableView::customContextMenuRequested, this, &NetEndpointTable::handleContextMenuRequested);
    }

    void NetEndpointTable::setNet(u32 netID)
    {
        Net* n = gNetlist->get_net_by_id(netID);

        setNet(n);
    }

    void NetEndpointTable::setNet(Net* n)
    {
        if (!n)
            return;

        mEndpointModel->setNet(n);

        QModelIndex unused;

        fitSizeToContent();

        Q_EMIT updateText(mEndpointModel->typeString() + " (" + QString::number(mEndpointModel->rowCount(unused)) + ")");
    }

    void NetEndpointTable::removeContent()
    {
        mEndpointModel->clear();
    }

    void NetEndpointTable::handleContextMenuRequested(const QPoint& pos)
    {
        QModelIndex idx = indexAt(pos);

        if(!idx.isValid())
            return;

        u32 gateID = mEndpointModel->getGateIDFromIndex(idx);

        QMenu menu;

        //menu.addSection("Python");

        QString pythonCommandGate = PyCodeProvider::pyCodeGate(gateID);
        menu.addAction(QIcon(":/icons/python"), "Extract gate as python code",
            [pythonCommandGate]()
            {
                QApplication::clipboard()->setText(pythonCommandGate);
            });

        menu.move(mapToGlobal(pos));
        menu.exec();
    }

    void NetEndpointTable::fitSizeToContent()
    {
        horizontalHeader()->setStretchLastSection(false);
        resizeRowsToContents();
        resizeColumnsToContents();

        int rows = mEndpointModel->rowCount();
        int columns = mEndpointModel->columnCount();

        int w = 0;
        int h = 0;

        //necessary to test if the table is empty, otherwise (due to the resizeColumnsToContents function)
        //is the table's width far too big, so just set 0 as the size
        if(rows != 0)
        {
            w = verticalHeader()->width() + 4;    // +4 seems to be needed
            for (int i = 0; i < columns; i++)
                w += columnWidth(i);    // seems to include gridline

            h = horizontalHeader()->height() + 4;
            for (int i = 0; i < rows; i++)
                h += rowHeight(i);

            w = w + 5; //no contemporary source exists why 5 is the magic number here (my guess would be it's the width of the hidden scrollbar)
        }
        setFixedHeight(h);
        setMinimumWidth(w);
        horizontalHeader()->setStretchLastSection(true);
        update();
        updateGeometry();
    }
}
