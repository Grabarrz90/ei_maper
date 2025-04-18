#include "undo.h"
#include "node.h"
#include "main_window.h"
#include "mob\mob.h"
#include "objects/unit.h"
#include "objects/magictrap.h"
#include "options.h"
#include "settings.h"
#include "log.h"
#include "property.h"


COpenCommand::COpenCommand(CView* pView, QFileInfo& path, QUndoCommand *parent) : QUndoCommand(parent)
    ,m_pView(pView)
    ,m_filePath(path)
{
    setText("Load: " + path.fileName());
}

void COpenCommand::undo()
{
    if(m_filePath.fileName().toLower().endsWith(".mpr"))
    {
        m_pView->unloadLand();
    }
    else
    {
        m_pView->unloadMob(m_filePath.fileName());
    }
}
void COpenCommand::redo()
{
    if(m_filePath.fileName().toLower().endsWith(".mpr"))
        m_pView->loadLandscape(m_filePath);
    else
    {
        m_pView->loadMob(m_filePath);
    }

}

//CChangeModelParam::CChangeModelParam(CView* pView, uint nodeId, EObjParam &objParam, QString value, QUndoCommand *parent):
//    CChangeStringParam(pView, nodeId, objParam, value, parent)
//{
//    //setText("Change model to " + value);
//}

//void CChangeModelParam::undo()
//{
//    auto pNode = m_pView->currentMob()->nodeByMapId(m_nodeId);
//    pNode->applyParam(m_objParam, m_oldValue);
//    pNode->applyParam(eObjParam_BODYPARTS, util::makeString(m_oldBodyparts));
//    emit updatePosOnLand(pNode);
//    emit updateParam();
//    m_pView->setDurty();
//}

//void CChangeModelParam::redo()
//{
//    setText("Change model to " + m_newValue);
//    auto pNode = m_pView->currentMob()->nodeByMapId(m_nodeId);
//    m_oldValue = pNode->getParam(m_objParam);
//    QString partsList = pNode->getParam(eObjParam_BODYPARTS);
//    m_oldBodyparts = util::strListFromString(partsList);
//    pNode->applyParam(m_objParam, m_newValue);
//    QString empty("");
//    pNode->applyParam(eObjParam_BODYPARTS, empty);
//    emit updatePosOnLand(pNode);
//    emit updateParam();
//    m_pView->setDurty();
//}

CDeleteNodeCommand::CDeleteNodeCommand(CView* pView, uint nodeId, QUndoCommand *parent)
    :QUndoCommand(parent)
    ,m_pView(pView)
    ,m_nodeId(nodeId)
{
}

void CDeleteNodeCommand::undo()
{
    auto pNode = m_pView->currentMob()->undo_deleteNode(m_nodeId);
    if(nullptr != pNode)
        emit undo_deleteNodeSignal(pNode);
    m_pView->setDurty();
}

void CDeleteNodeCommand::redo()
{
    auto pNode = m_pView->currentMob()->nodeByMapId(m_nodeId);
    setText(QString("Delete node ID: %1").arg(pNode->mapId()));
    ei::log(eLogInfo, "Delete node ID: " + QString::number(pNode->mapId()));
    m_pView->currentMob()->deleteNode(m_nodeId);
    emit deleteNodeSignal(m_nodeId);
    m_pView->setDurty();
}

CCreateNodeCommand::CCreateNodeCommand(CView* pView, QJsonObject nodeData, QUndoCommand *parent):
    QUndoCommand(parent)
    ,m_pView(pView)
    ,m_nodeData(nodeData)
    ,m_createdNodeId(0)
{
    //setText("Node created");
}

void CCreateNodeCommand::undo()
{
    m_pView->currentMob()->undo_createNode(m_createdNodeId);
    m_pView->setDurty();
    emit undo_addNodeSignal(m_createdNodeId);
}

void CCreateNodeCommand::redo()
{
    auto pNode = m_pView->currentMob()->createNode(m_nodeData);
    m_createdNodeId = pNode->mapId();
    setText("Node created ID: " + QString::number(pNode->mapId()));
    m_pView->setDurty();
    emit addNodeSignal(pNode);
}

CChangeLogicParam::CChangeLogicParam(CView* pView, QString pointHash, const QSharedPointer<IPropertyBase>& prop, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_pointHash(pointHash)
{
    m_newValue.reset(prop->clone());
}

void CChangeLogicParam::undo()
{
    QStringList list = m_pointHash.split(".");
    if(list.size()==1)
    {//unit
        auto pUnit = m_pView->currentMob()->nodeByMapId(list[0].toUInt());
        pUnit->applyLogicParam(m_oldValue);
    }
    else if(list.size()==2)
    {//patrol point
        CPatrolPoint* pPoint = m_pView->currentMob()->patrolPointById(list[0].toInt(), list[1].toInt());
        pPoint->applyLogicParam(m_oldValue);
    }
    else if(list.size()==3)
    {//look point
        CLookPoint* pPoint = m_pView->currentMob()->viewPointById(list[0].toInt(), list[1].toInt(), list[2].toInt());
        pPoint->applyLogicParam(m_oldValue);
    }
    else if(list.size() == 4) //trap zone
    {
        CActivationZone* pZone = m_pView->currentMob()->actZoneById(list[0].toInt(), list[3].toInt());
        pZone->applyLogicParam(m_oldValue);
    }
    else if(list.size() == 5) //trap cast point
    {
        CTrapCastPoint* pCast = m_pView->currentMob()->trapCastById(list[0].toInt(), list[4].toInt());
        pCast->applyLogicParam(m_oldValue);
    }

    emit updateParam();
    m_pView->setDurty();
}

void CChangeLogicParam::redo()
{
    QStringList list = m_pointHash.split(".");
    if(list.size()==1)
    {//unit
        auto pUnit = m_pView->currentMob()->nodeByMapId(list[0].toUInt()); //can be magic trap
        pUnit->getLogicParam(m_oldValue, m_newValue->type());
        pUnit->applyLogicParam(m_newValue);
    }
    else if(list.size()==2)
    {//patrol point
        CPatrolPoint* pPoint = m_pView->currentMob()->patrolPointById(list[0].toInt(), list[1].toInt());
        pPoint->getLogicParam(m_oldValue, m_newValue->type());
        pPoint->applyLogicParam(m_newValue);
    }
    else if(list.size()==3)
    {//look point
        CLookPoint* pPoint = m_pView->currentMob()->viewPointById(list[0].toInt(), list[1].toInt(), list[2].toInt());
        pPoint->getLogicParam(m_oldValue, m_newValue->type());
        pPoint->applyLogicParam(m_newValue);
    }
    else if(list.size() == 4) //trap zone
    {
        CActivationZone* pZone = m_pView->currentMob()->actZoneById(list[0].toInt(), list[3].toInt());
        pZone->getLogicParam(m_oldValue, m_newValue->type());
        pZone->applyLogicParam(m_newValue);
    }
    else if(list.size() == 5) //trap cast point
    {
        CTrapCastPoint* pCast = m_pView->currentMob()->trapCastById(list[0].toInt(), list[4].toInt());
        pCast->getLogicParam(m_oldValue, m_newValue->type());
        pCast->applyLogicParam(m_newValue);
    }

    emit updateParam();
    m_pView->setDurty();
    setText("Change value to " + m_newValue->toString());
}

CCreatePatrolCommand::CCreatePatrolCommand(CView* pView, QString pointHash, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_pointHash(pointHash)
{
}

void CCreatePatrolCommand::undo()
{
    m_pView->currentMob()->undo_createPatrolByHash(m_pointHash);
}

void CCreatePatrolCommand::redo()
{
    m_pView->currentMob()->createPatrolByHash(m_pointHash);
    setText("Created new point");
}

CCreateTrapPointCommand::CCreateTrapPointCommand(CView *pView, uint trapId, bool bActZone, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_trapId(trapId)
  ,m_bActZone(bActZone)
{
}

void CCreateTrapPointCommand::undo()
{
    CMagicTrap* pTrap = dynamic_cast<CMagicTrap*>(m_pView->currentMob()->nodeByMapId(m_trapId));
    if(m_bActZone)
        pTrap->deleteLastActZone();
    else
        pTrap->deleteLastCastPoint();
    m_pView->currentMob()->logicNodesUpdate();
}

void CCreateTrapPointCommand::redo()
{
    CMagicTrap* pTrap = dynamic_cast<CMagicTrap*>(m_pView->currentMob()->nodeByMapId(m_trapId));
    if(m_bActZone)
    {
        auto pZone = pTrap->createActZone();
        pZone->setState(ENodeState::eSelect);
    }
    else
    {
        auto pCast = pTrap->createCastPoint();
        pCast->setState(ENodeState::eSelect);
    }
    m_pView->currentMob()->logicNodesUpdate();
}

CDeleteLogicPoint::CDeleteLogicPoint(CView *pView, QString hash, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_nodeHash(hash)
{
}

void CDeleteLogicPoint::undo()
{
    QStringList list = m_nodeHash.split(".");
    if(list.size()==1)
    {
        Q_ASSERT(false);
    }
    else if(list.size()==2)
    {//patrol point
        CPatrolPoint* pPoint = m_pView->currentMob()->patrolPointById(list[0].toInt(), list[1].toInt());
        pPoint->markAsDeleted(false);
    }
    else if(list.size()==3)
    {//look point
        CLookPoint* pPoint = m_pView->currentMob()->viewPointById(list[0].toInt(), list[1].toInt(), list[2].toInt());
        pPoint->markAsDeleted(false);
    }
    else if(list.size() == 4) //trap zone
    {
        CActivationZone* pZone = m_pView->currentMob()->actZoneById(list[0].toInt(), list[3].toInt());
        pZone->markAsDeleted(false);
    }
    else if(list.size() == 5) //trap cast point
    {
        CTrapCastPoint* pCast = m_pView->currentMob()->trapCastById(list[0].toInt(), list[4].toInt());
        pCast->markAsDeleted(false);
    }
}

void CDeleteLogicPoint::redo()
{
    QStringList list = m_nodeHash.split(".");
    if(list.size()==1)
    {
        Q_ASSERT(false);
    }
    else if(list.size()==2)
    {//patrol point
        CPatrolPoint* pPoint = m_pView->currentMob()->patrolPointById(list[0].toInt(), list[1].toInt());
        pPoint->markAsDeleted(true);
    }
    else if(list.size()==3)
    {//look point
        CLookPoint* pPoint = m_pView->currentMob()->viewPointById(list[0].toInt(), list[1].toInt(), list[2].toInt());
        pPoint->markAsDeleted(true);
    }
    else if(list.size() == 4) //trap zone
    {
        CActivationZone* pZone = m_pView->currentMob()->actZoneById(list[0].toInt(), list[3].toInt());
        pZone->markAsDeleted(true);
    }
    else if(list.size() == 5) //trap cast point
    {
        CTrapCastPoint* pCast = m_pView->currentMob()->trapCastById(list[0].toInt(), list[4].toInt());
        pCast->markAsDeleted(true);
    }
    setText("Point deleted");
    m_pView->setDurty();
}

CRoundMobCommand::CRoundMobCommand(CView *pView, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
{
}

void CRoundMobCommand::undo()
{
    m_pView->undo_roundActiveMob();
}

void CRoundMobCommand::redo()
{
    m_pView->roundActiveMob();
    setText("Switch Mob to " + m_pView->currentMob()->mobName());
}

CCloseActiveMobCommand::CCloseActiveMobCommand(CView *pView, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
{
    CMob* pMob = m_pView->currentMob();
    if(nullptr != pMob)
        m_filePath = pMob->filePath();
}

void CCloseActiveMobCommand::undo()
{
    m_pView->loadMob(m_filePath);
    m_pView->changeCurrentMob(m_pView->mob(m_filePath.fileName()));
}

void CCloseActiveMobCommand::redo()
{
    CMob* pMob = m_pView->currentMob();
    if(nullptr != pMob)
    {
        setText("unloaded " + pMob->mobName());
        m_pView->unloadActiveMob();
    }
}

CSwitchToQuestMobCommand::CSwitchToQuestMobCommand(CMob* pMob, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pMob(pMob)
  ,m_userAnswer(QMessageBox::NoButton)
{
    m_bQuestMob = m_pMob->isQuestMob();
    m_oldWS = m_pMob->worldSet();
    m_arrOldMnR = m_pMob->ranges(true);
    m_arrOldScR = m_pMob->ranges(false);
    m_oldDiplomacyFoF = m_pMob->diplomacyField();
    m_arrOldDiplomacyFieldName = m_pMob->diplomacyNames();
}

void CSwitchToQuestMobCommand::undo()
{
    m_pMob->setQuestMob(m_bQuestMob);
    m_pMob->setWorldSet(m_oldWS);
    m_pMob->setRanges(true, m_arrOldMnR);
    m_pMob->setRanges(false, m_arrOldScR);
    m_pMob->setDiplomacyField(m_oldDiplomacyFoF);
    m_pMob->setDiplomacyNames(m_arrOldDiplomacyFieldName);
    emit switchQuestMobSignal();
}

void CSwitchToQuestMobCommand::redo()
{
    auto arrRange = m_pMob->ranges(!m_bQuestMob);
    if(!arrRange.isEmpty())
    {
        if(m_userAnswer == QMessageBox::NoButton)
            m_userAnswer = QMessageBox::question(nullptr, "Switching MOB Type", "Do you want to switch Id ranges as well?", QMessageBox::Yes|QMessageBox::No);

        if(m_userAnswer == QMessageBox::Yes)
            m_pMob->setRanges(m_bQuestMob, arrRange);
    }

    CWorldSet ws;
    m_pMob->setWorldSet(ws);
    m_pMob->clearRanges(!m_bQuestMob);
    if(m_bQuestMob)
        m_pMob->generateDiplomacyTable();
    else
        m_pMob->clearDiplomacyTable();

    m_pMob->setQuestMob(!m_bQuestMob);
    setText(m_bQuestMob ? "Quest MOB to Base" : "Base MOB to Quest");
    emit switchQuestMobSignal();
}

CChangeWorldSetCommand::CChangeWorldSetCommand(CMob *pMob, EWsType paramType, QString &value, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pMob(pMob)
  ,m_paramType(paramType)
  ,m_newValue(value)
{
    CWorldSet ws = m_pMob->worldSet();
    m_oldValue = ws.data(paramType);
}

void CChangeWorldSetCommand::undo()
{
    CWorldSet ws = m_pMob->worldSet();
    ws.setData(m_paramType, m_oldValue);
    m_pMob->setWorldSet(ws);
    emit changeWsSignal();
}

void CChangeWorldSetCommand::redo()
{
    CWorldSet ws = m_pMob->worldSet();
    ws.setData(m_paramType, m_newValue);
    m_pMob->setWorldSet(ws);
    setText("Changes value to" + m_newValue);
    emit changeWsSignal();
}

CChangeRangeCommand::CChangeRangeCommand(CMob *pMob, int index, SRange range, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pMob(pMob)
  ,m_index(index)
  ,m_newRange(range)
{
    auto arrRanges = m_pMob->ranges(!m_pMob->isQuestMob());
    if(m_index < arrRanges.count())
    {
        m_oldRange = arrRanges[index];
    }
}

void CChangeRangeCommand::undo()
{
    auto arrRanges(m_pMob->ranges(!m_pMob->isQuestMob()));
    if(m_newRange.isEmpty()) //undo deleting
        arrRanges.insert(m_index, m_oldRange);
    else if(m_oldRange.isEmpty()) // undo appending
        arrRanges.pop_back();
    else //undo changing
        arrRanges[m_index] = m_oldRange;

    m_pMob->setRanges(!m_pMob->isQuestMob(), arrRanges);
    emit changeRangeSignal();
}

void CChangeRangeCommand::redo()
{
    auto arrRanges(m_pMob->ranges(!m_pMob->isQuestMob()));
    if(m_index < arrRanges.count())
    {
        if(m_newRange.isEmpty())
        {
            setText("Deleted range " + QString::number(m_oldRange.minRange) + " - " + QString::number(m_oldRange.maxRange));
            arrRanges.takeAt(m_index);
        }
        else
        {
            setText("Changed range to " + QString::number(m_newRange.minRange) + " - " + QString::number(m_newRange.maxRange));
            arrRanges[m_index] = m_newRange;
        }
    }
    else
    {
        arrRanges.append(m_newRange);
        setText("Added range " + QString::number(m_newRange.minRange) + " - " + QString::number(m_newRange.maxRange));
    }
    m_pMob->setRanges(!m_pMob->isQuestMob(), arrRanges);
    emit changeRangeSignal();
}

CChangeDiplomacyTableItem::CChangeDiplomacyTableItem(CMob *pMob, int row, int column, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pMob(pMob)
  ,m_row(row)
  ,m_column(column)
  ,m_bSymmetric(true)
{
    COptBool* pOpt = dynamic_cast<COptBool*>(pMob->view()->settings()->opt("dipEditSymmetric"));
    if (pOpt)
        m_bSymmetric = pOpt->value();
}

void CChangeDiplomacyTableItem::undo()
{
    if (m_oldValue < 0)
        return;

    QVector<QVector<uint>>& dipTable = m_pMob->diplomacyField();
    if(dipTable.isEmpty())
    {
        m_oldValue = -1;
        return;
    }
    dipTable[m_row][m_column] = m_oldValue;
    dipTable[m_column][m_row] = m_oldValueSymetric;
    emit changeDipGroup(m_row, m_column);
    emit changeDipGroup(m_column, m_row);
}

void CChangeDiplomacyTableItem::redo()
{
    QVector<QVector<uint>>& dipTable = m_pMob->diplomacyField();
    if(dipTable.isEmpty())
    {
        m_oldValue = -1;
        return;
    }
    m_oldValue = dipTable[m_row][m_column];
    m_oldValueSymetric = dipTable[m_column][m_row];
    int value = m_oldValue;
    ++ value;
    value = value % 3;
    dipTable[m_row][m_column] = value;
    emit changeDipGroup(m_row, m_column);
    if(m_bSymmetric)
    {
        dipTable[m_column][m_row] = value;
        emit changeDipGroup(m_column, m_row);
    }
    setText("Dip group (" + QString::number(m_row) + ", " + QString::number(m_column) + ") changed to " + QString::number(value));
}

CChangeActiveMobCommand::CChangeActiveMobCommand(CView *pView, QString mobName, QUndoCommand *parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_mobName(mobName)
{
    m_oldMobName = m_pView->currentMob()->mobName();
}

void CChangeActiveMobCommand::undo()
{
    m_pView->changeCurrentMob(m_oldMobName);
}

void CChangeActiveMobCommand::redo()
{
    m_pView->changeCurrentMob(m_mobName);
    setText("Change MOB to:" + m_mobName);
}

CChangeProp::CChangeProp(CView* pView, uint nodeId, const QSharedPointer<IPropertyBase>& prop, QUndoCommand* parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_nodeId(nodeId)
{
    m_newValue.reset(prop->clone());
}

void CChangeProp::undo()
{
    auto pNode = m_pView->currentMob()->nodeByMapId(m_nodeId);
    pNode->applyParam(m_oldValue);
    if(m_oldValue->type() == eObjParam_NID)
    {
        m_nodeId = dynamic_cast<propUint*>(m_oldValue.get())->value();
        uint id = dynamic_cast<propUint*>(m_newValue.get())->value();
        emit changeIdSignal(id, m_nodeId);
    }
    else if(m_oldValue->type() == eObjParam_NAME || m_oldValue->type() == eObjParam_UNIT_PROTOTYPE)
    {
        emit changeTreeName(pNode);
    }
    emit updateParam();
    //emit updatePosOnLand(m_pNode);
    m_pView->setDurty();
}

void CChangeProp::redo()
{
    setText("Change value to " + m_newValue->toString());
    auto pNode = m_pView->currentMob()->nodeByMapId(m_nodeId);
    pNode->getParam(m_oldValue, m_newValue->type());
    pNode->applyParam(m_newValue);
    switch (m_newValue->type()) {
    case eObjParam_NID:
    {
        //todo: check if changes allowed
        uint oldNodeId = dynamic_cast<propUint*>(m_oldValue.get())->value();
        m_nodeId = dynamic_cast<propUint*>(m_newValue.get())->value();
        emit changeIdSignal(oldNodeId, m_nodeId);
        break;
    }
    case eObjParam_NAME:
    case eObjParam_UNIT_PROTOTYPE:
    {
        emit changeTreeName(pNode);
        break;
    }
    default: {break;}
    }

    emit updateParam();
    //emit updatePosOnLand(m_pNode);
    m_pView->setDurty();
}

CResetIdCommand::CResetIdCommand(CView* pView, QMap<uint, uint>& reconnectId, QUndoCommand* parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_reconnectId(reconnectId)
{

}

void CResetIdCommand::undo()
{
    CNode* pNode = nullptr;
    foreach(pNode, m_pView->currentMob()->nodes())
    {
        if(!m_reconnectId.values().contains(pNode->mapId()))
            continue;
        pNode->setMapId(m_reconnectId.key(pNode->mapId()));
    }
    emit updateParam();
}

void CResetIdCommand::redo()
{
    setText("reset IDs for "+QString::number(m_reconnectId.size())+" elems");
    CNode* pNode = nullptr;
    foreach(pNode, m_pView->currentMob()->nodes())
    {
        if(!m_reconnectId.contains(pNode->mapId()))
            continue;
        pNode->setMapId(m_reconnectId[pNode->mapId()]);
    }
    emit updateParam();
}

CBrushTileCommand::CBrushTileCommand(CView* pView, STileInfo tileInfoNew, STileLocation tileLoc, STileInfo tileInfoOld, short tileBrushCommandId, QUndoCommand* parent):
    QUndoCommand(parent)
  ,m_pView(pView)
  ,m_tileInfoNew(tileInfoNew)
  ,m_tileLoc(tileLoc)
  ,m_tileInfoOld(tileInfoOld)
  ,m_tileBrushCommandId(tileBrushCommandId)
{
    m_arrTileBrushNew[tileLoc] = tileInfoNew;
    m_arrTileBrushOld[tileLoc] = tileInfoOld;
}

void CBrushTileCommand::undo()
{
    m_pView->setTile(m_arrTileBrushOld);
}

void CBrushTileCommand::redo()
{
    setText("Brush tiles ("+QString::number(m_arrTileBrushNew.size())+")");
    m_pView->setTile(m_arrTileBrushNew);
    m_pView->setDirtyMpr();
}

bool CBrushTileCommand::mergeWith(const QUndoCommand* command)
{
    const CBrushTileCommand* otherCmd = dynamic_cast<const CBrushTileCommand*>(command);
    if (otherCmd && otherCmd->m_tileInfoNew == m_tileInfoNew && m_tileBrushCommandId == otherCmd->m_tileBrushCommandId)
    {
        if(!m_arrTileBrushNew.contains(otherCmd->m_tileLoc))
        {
            m_arrTileBrushNew[otherCmd->m_tileLoc] = otherCmd->m_tileInfoNew;
            m_arrTileBrushOld[otherCmd->m_tileLoc] = otherCmd->m_tileInfoOld;
        }
        setText("Brush tiles ("+QString::number(m_arrTileBrushNew.size())+")");
        return true;
    }
    return false;
}
