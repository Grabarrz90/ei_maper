#ifndef OBJECT_H
#define OBJECT_H

#include <QVector>
#include <QGLWidget>
#include <QOpenGLFunctions>
//#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
//#include <QOpenGLTexture>
#include "node.h"
#include "part.h"

class CMob;

class CObjectBase : public CNode
{
public:
    CObjectBase();
    CObjectBase(const CObjectBase& base);
    CObjectBase(QJsonObject data);
    CObjectBase(CNode* node);
    ~CObjectBase() override;

    void draw(bool isActive, QOpenGLShaderProgram* program) override;
    void drawSelect(QOpenGLShaderProgram* program = nullptr) override;
    ENodeType nodeType() override {return ENodeType::eBaseType; }    //todo: delete, use this class as abstract type
    QString& modelName() override {return m_modelName; }
    QString textureName() override {return QString("default0");}
    void updateFigure(ei::CFigure* fig) override;
    void loadFigure() override;
    void loadTexture() override;
    void setModelName(QString name) {m_modelName = name;}
    QVector3D& minPosition() override { return m_minPoint; }
    void setTexture(QOpenGLTexture* texture) override;
    void serializeJson(QJsonObject& obj) override;
    uint serialize(util::CMobParser& parser) override;
    void collectParams(QList<QSharedPointer<IPropertyBase>>& aProp, ENodeType paramType) override;
    void collectlogicParams(QList<QSharedPointer<IPropertyBase>>& aProp, ENodeType paramType) override;
    void getParam(QSharedPointer<IPropertyBase>& prop, EObjParam propType) override;
    void applyParam(const QSharedPointer<IPropertyBase>& prop) override;
    void getLogicParam(QSharedPointer<IPropertyBase>& prop, EObjParam propType) override;
    void applyLogicParam(const QSharedPointer<IPropertyBase>& prop) override;
    bool updatePos(QVector3D& pos) override;
    void setRot(const QQuaternion& quat) override;
    const QVector3D& constitution() override final {return m_complection;}
    const QVector3D& complection() override {return m_complection;}
    void setConstitution(QVector3D& vec) override;
    QJsonObject toJson() override;
    CBox getBBox() override final;
    void markAsDeleted(bool bDeleted = true) override {m_bDeleted = bDeleted;}
    bool isMarkDeleted() override {return m_bDeleted;}
    bool isOperationAxisAllow(EOperationAxisType type) override {Q_UNUSED(type); return true;};

protected:
    void recalcFigure();
    void recalcMinPos();
    //void updateVisibility(QVector<QString>& aPart);

protected:
    QString m_modelName;
    QVector3D m_complection;    //x-grace, y-strength, z-tall
    QStringList m_bodyParts; // for preparing vertex data

private:
    QVector<CPart*> m_aPart;
    QVector3D m_minPoint;   // min position of model bound boxes(units half body under ground)
    QOpenGLTexture* m_texture;
    ei::CFigure* m_pFigure;
    bool m_bDeleted;
};

#endif // OBJECT_H
