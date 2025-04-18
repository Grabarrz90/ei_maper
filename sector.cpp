#include <QtGlobal>

#include "sector.h"
#include "landscape.h"
#include "math_utils.h"
#include "log.h"
#include "utils.h"

static const uint secSignature = 0xcf4bf774;
static const int nVertex = 33; // vertices count by 1 side of sector
//const int nSecVertex = nVertex * nVertex;
static const int nTile = 16;   //tiles count by 1 side of sector
//static constexpr int nSecTile = nTile * nTile;

CSector::~CSector()
{
    m_vertexBuf.destroy();
    m_indexBuf.destroy();
    m_waterVertexBuf.destroy();
    m_waterIndexBuf.destroy();
}

CSector::CSector(QDataStream& stream, float maxZ, int texCount)
    :m_indexBuf(QOpenGLBuffer::IndexBuffer)
    ,m_waterIndexBuf(QOpenGLBuffer::IndexBuffer)
{
    uint signature;
    stream >> signature;
    if(signature != secSignature)
    {
        ei::log(eLogFatal, "Incorrect sector signature");
        return;
    }
    //init opengl buffers
    m_vertexBuf.create();
    m_indexBuf.create();
    m_waterVertexBuf.create();
    m_waterIndexBuf.create();
    m_modelMatrix.setToIdentity();

    //continue read *.sec data
    quint8 secType;
    stream >> secType;
    const bool bWater = secType == 3;
    m_arrLand.clear();
    SSecVertex sv;
    QVector<QVector<SSecVertex>> landVertex;
    QVector<SSecVertex> aLine;
    for (int column(0); column<nVertex; ++column)
    {
        aLine.clear();
        for(int row(0); row<nVertex; ++row)
        {
            stream >> sv;
            aLine.append(sv);
        }
        landVertex.append(aLine);
    }

    QVector<QVector<SSecVertex>> waterVertex;
    if(bWater)
    {
        for (int column(0); column<nVertex; ++column)
        {
            aLine.clear();
            for(int row(0); row<nVertex; ++row)
            {
                stream >> sv;
                aLine.append(sv);
            }
            waterVertex.append(aLine);
        }
    }

    // convert filedata to tile-specific quads. x,y(0,0) is left down corner. Move to right by column then up by row
    m_arrLand.resize(nTile);
    QVector<SSecVertex> arrVertex;
    for(int i(0); i<nTile; ++i)
    {
        m_arrLand[i].resize(nTile);
    }

    QVector<STile> aLandTiles; //todo: remove
    ushort tilePacked;
    for (int row(0); row<nTile; ++row)
    {
        for(int col(0); col<nTile; ++col)
        {
            stream >> tilePacked;
            STile tile(tilePacked); //todo: remove
            aLandTiles.append(tile); //todo: remove
            m_arrLand[row][col] = CTile(tilePacked, col*2, row*2, maxZ, texCount);

            arrVertex.clear();
            arrVertex.resize(9);
            // step for vertices = 2*step for tiles
            arrVertex[0] = landVertex[row*2][col*2];
            arrVertex[1] = landVertex[row*2][col*2+1];
            arrVertex[2] = landVertex[row*2][col*2+2];
            arrVertex[3] = landVertex[row*2+1][col*2];
            arrVertex[4] = landVertex[row*2+1][col*2+1];
            arrVertex[5] = landVertex[row*2+1][col*2+2];
            arrVertex[6] = landVertex[row*2+2][col*2];
            arrVertex[7] = landVertex[row*2+2][col*2+1];
            arrVertex[8] = landVertex[row*2+2][col*2+2];
            m_arrLand[row][col].resetVertices(arrVertex);
        }
    }

    //QVector<STile> aWaterTiles;
    if(bWater)
    {
        m_arrWater.resize(nTile);
        QVector<SSecVertex> arrVertex;
        for(int i(0); i<nTile; ++i)
        {
            m_arrWater[i].resize(nTile);
        }

        for (int row(0); row<nTile; ++row)
        {
            for(int col(0); col<nTile; ++col)
            {
                stream >> tilePacked;
                m_arrWater[row][col] = CTile(tilePacked, col*2, row*2, maxZ, texCount);

                arrVertex.clear();
                arrVertex.resize(9);
                // step for vertices = 2*step for tiles
                arrVertex[0] = waterVertex[row*2][col*2];
                arrVertex[1] = waterVertex[row*2][col*2+1];
                arrVertex[2] = waterVertex[row*2][col*2+2];
                arrVertex[3] = waterVertex[row*2+1][col*2];
                arrVertex[4] = waterVertex[row*2+1][col*2+1];
                arrVertex[5] = waterVertex[row*2+1][col*2+2];
                arrVertex[6] = waterVertex[row*2+2][col*2];
                arrVertex[7] = waterVertex[row*2+2][col*2+1];
                arrVertex[8] = waterVertex[row*2+2][col*2+2];
                m_arrWater[row][col].resetVertices(arrVertex);
            }
        }

        short matIndex;
        for (int row(0); row<nTile; ++row)
        {
            for(int col(0); col<nTile; ++col)
            {
                stream >> matIndex;
                m_arrWater[row][col].setMaterialIndex(matIndex);
            }
        }

    }

    //makeVertexData(landVertex, aLandTiles, waterVertex, aWaterTiles, maxZ, texCount);
    generateVertexDataFromTile();
}

QByteArray CSector::serializeSector()
{
    QByteArray secData;
    QDataStream secStream(&secData, QIODevice::WriteOnly);
    util::formatStream(secStream);
    bool bWater = !m_arrWater.isEmpty();
    secStream << secSignature;
    secStream << (bWater ? quint8(3) : quint8(0));

    //generate SSecVertex line by line
    QVector<SSecVertex> arrVrt;
    arrVrt.resize(nVertex*nVertex);
    QVector<ushort> arrLandTilePacked;
    arrLandTilePacked.resize(nTile*nTile);
    //water tiles
    QVector<SSecVertex> arrWaterVrt;
    QVector<ushort> arrWaterTilePacked;
    QVector<short> arrWaterMaterial;
    if(bWater)
    {
        arrWaterVrt.resize(nVertex*nVertex);
        arrWaterTilePacked.resize(nTile*nTile);
        arrWaterMaterial.resize(nTile*nTile);
    }

    auto tileToLine = [](QVector<SSecVertex>& outVrt, const QVector<QVector<SSecVertex>>& arr, int row, int col)
    { // converts 3x3 tile vertex data to 1 line of data
        outVrt[(row*2+0)*nVertex + col*2 + 0] = arr[0][0];
        outVrt[(row*2+0)*nVertex + col*2 + 1] = arr[0][1];
        outVrt[(row*2+1)*nVertex + col*2 + 0] = arr[1][0];
        outVrt[(row*2+1)*nVertex + col*2 + 1] = arr[1][1];
        if(col == nTile-1)
        { // last tile in row, write right side of tile
            outVrt[(row*2+0)*nVertex + col*2 + 2] = arr[0][2];
            outVrt[(row*2+1)*nVertex + col*2 + 2] = arr[1][2];
        }
        if(row == nTile-1)
        { // last tile row, write top side
            outVrt[(row*2+2)*nVertex + col*2 + 0] = arr[2][0];
            outVrt[(row*2+2)*nVertex + col*2 + 1] = arr[2][1];
            if(col == nTile-1)
            { // last tile of sector, write right side of tile
                outVrt[(row*2+0)*nVertex + col*2 + 2] = arr[0][2];
                outVrt[(row*2+1)*nVertex + col*2 + 2] = arr[1][2];
                outVrt[(row*2+2)*nVertex + col*2 + 2] = arr[2][2];
            }
        }
    };

    for(int row(0); row<nTile; ++row)
        for(int col(0); col<nTile; ++col)
        {
            tileToLine(arrVrt, m_arrLand[row][col].arrVertex(), row, col);
            arrLandTilePacked[row*nTile + col] = m_arrLand[row][col].packData();
            if(bWater)
            {
                tileToLine(arrWaterVrt, m_arrWater[row][col].arrVertex(), row, col);
                arrWaterTilePacked[row*nTile + col] = m_arrWater[row][col].packData();
                arrWaterMaterial[row*nTile + col] = m_arrWater[row][col].materialIndex();
            }
        }

    for(auto& vrt: arrVrt)
        secStream << vrt;

    if(bWater)
        for(auto& vrt: arrWaterVrt)
            secStream << vrt;

    for(auto& tile: arrLandTilePacked)
        secStream << tile;

    if(bWater)
    {
        for(auto& tile: arrWaterTilePacked)
            secStream << tile;

        for(auto& mat: arrWaterMaterial)
            secStream << mat;
    }

    return secData;
}

void SSpecificQuad::rotate(int step)
{
    Q_ASSERT(m_aVertexPointer.size() == 16);
    QVector<QVector2D> aTmpRow;
    aTmpRow.resize(4);
    for(int i(0); i<step; ++i)
    {
        aTmpRow[0] = m_aVertexPointer[0]->texCoord;
        aTmpRow[1] = m_aVertexPointer[1]->texCoord;
        aTmpRow[2] = m_aVertexPointer[2]->texCoord;
        aTmpRow[3] = m_aVertexPointer[3]->texCoord;

        m_aVertexPointer[0]->texCoord = m_aVertexPointer[5]->texCoord;
        m_aVertexPointer[1]->texCoord = m_aVertexPointer[6]->texCoord;
        m_aVertexPointer[2]->texCoord = m_aVertexPointer[7]->texCoord;
        m_aVertexPointer[3]->texCoord = m_aVertexPointer[4]->texCoord;

        m_aVertexPointer[4]->texCoord  = m_aVertexPointer[13]->texCoord;
        m_aVertexPointer[5]->texCoord  = m_aVertexPointer[14]->texCoord;
        m_aVertexPointer[6]->texCoord = m_aVertexPointer[15]->texCoord;
        m_aVertexPointer[7]->texCoord = m_aVertexPointer[12]->texCoord;

        m_aVertexPointer[12]->texCoord = m_aVertexPointer[9]->texCoord;
        m_aVertexPointer[13]->texCoord = m_aVertexPointer[10]->texCoord;
        m_aVertexPointer[14]->texCoord = m_aVertexPointer[11]->texCoord;
        m_aVertexPointer[15]->texCoord = m_aVertexPointer[8]->texCoord;

        m_aVertexPointer[8]->texCoord  = aTmpRow[1];
        m_aVertexPointer[9]->texCoord  = aTmpRow[2];
        m_aVertexPointer[10]->texCoord = aTmpRow[3];
        m_aVertexPointer[11]->texCoord = aTmpRow[0];
    }
    m_rotateState = step;
}

void CSector::makeVertexData(QVector<QVector<SSecVertex>>& aLandVertex, QVector<STile>& aLandTile,
                             QVector<QVector<SSecVertex>>& aWaterVertex, QVector<STile>& aWaterTile,
                             float maxZ, int texCount)
{
/*
^|   6 _7 _8
c|   |\ |\ |
o|   |_\|_\|
l|   3 _4 _5
u|   |\ |\ |
m|   |_\|_\|
n|   0  1  2 ROW---->
specific quad is
4 land quads -> 16 points -> 16 indexes,  {0,1,4,3}, {1,2,5,4}, {3,4,7,6}, {4,5,8,7}
1 tile -> 16 tex coords with the same indexes
TODO: use 9 vertex, make quad via index array
*/

    Q_ASSERT(aLandVertex.size() == 33);
    Q_ASSERT(aLandTile.size() == 256);
    QVector<QVector2D> tCoord; // min + max
    tCoord.resize(2);
    //SVertexData vData;
    m_arrLandVrtData.resize(aLandTile.size()*16);
    m_arrWaterVrtData.resize(aWaterTile.size()*16);
    int idVert(0);

    auto calcTexCoordBox = [&tCoord, texCount](STile& tile)
    {
      tCoord[0].setX((tile.m_index%8/8.0f+tile.m_texture)/texCount);
      tCoord[0].setY((7-int(tile.m_index/8))/8.0f);

      tCoord[1].setX(tCoord[0].x() + 1.0f/8.0f/texCount);
      tCoord[1].setY(tCoord[0].y() + 1.0f/8.0f);

      tCoord[0] += QVector2D(4.0f/512.0f/texCount, 4.0f/512.0f/texCount);
      tCoord[1] -= QVector2D(4.0f/512.0f/texCount, 4.0f/512.0f/texCount);
    };

    int vertIndexQuad(0);

    auto calcTexCoord = [&vertIndexQuad, &tCoord](SVertexData& vrt)
    {
        QVector2D texC = QVector2D(tCoord[0].x(), tCoord[1].y());
        switch(vertIndexQuad)
        {
        case 1:
        case 2:
        case 9:
        case 10:
        case 4:
        case 7:
        case 12:
        case 15:
        {
            texC.setX((tCoord[0].x() + tCoord[1].x())/2.0f);
            break;
        }
        case 5:
        case 6:
        case 13:
        case 14:
        {
            texC.setX(tCoord[1].x());
            break;
        }
        default:
            break;
        }

        switch (vertIndexQuad)
        {
        case 3:
        case 2:
        case 7:
        case 6:
        case 8:
        case 9:
        case 12:
        case 13:
        {
            texC.setY((tCoord[0].y() + tCoord[1].y())/2.0f);
            break;
        }
        case 11:
        case 10:
        case 15:
        case 14:
        {
            texC.setY(tCoord[0].y());
            break;
        }
        }

        vrt.texCoord = texC;
    };

    auto buildGeom = [maxZ, &vertIndexQuad, &calcTexCoord](QVector<QVector<SSecVertex>>& aVertex, SVertexData& vrt, int x, int y)
    {
        if(vertIndexQuad == 1 || vertIndexQuad == 2 || vertIndexQuad == 9 || vertIndexQuad == 10
            || vertIndexQuad == 4 || vertIndexQuad == 7 || vertIndexQuad == 12 || vertIndexQuad == 15)
            x+=1;
        else if(vertIndexQuad == 5 || vertIndexQuad == 6 || vertIndexQuad == 13 || vertIndexQuad == 14)
            x+=2;
        if(vertIndexQuad == 3 || vertIndexQuad == 2 || vertIndexQuad == 7 || vertIndexQuad == 6
            || vertIndexQuad == 8 || vertIndexQuad == 9 || vertIndexQuad == 12 || vertIndexQuad == 13)
            y+=1;
        else if(vertIndexQuad == 11 || vertIndexQuad == 10 || vertIndexQuad == 15 || vertIndexQuad == 14)
            y+=2;
        vrt.position.setX(x+aVertex[y][x].xOffset/254.0f);
        vrt.position.setY(y+aVertex[y][x].yOffset/254.0f);
        vrt.position.setZ(aVertex[y][x].z*maxZ/65535.0f);
        vrt.normal = aVertex[y][x].normal;
        calcTexCoord(vrt);
    };

    for(int i(0); i<aLandTile.size(); ++i)
    {
        calcTexCoordBox(aLandTile[i]);
        SSpecificQuad quad;
        int vrtX = i%16*2;
        int vrtY = int(i/16*2);
        for(vertIndexQuad = 0; vertIndexQuad < 16 ; ++vertIndexQuad)
        {
            buildGeom(aLandVertex, m_arrLandVrtData[idVert], vrtX, vrtY);
            quad.addVertex(&m_arrLandVrtData[idVert]);
            ++idVert;
        }
        quad.rotate(aLandTile[i].m_rotation);
    }

    idVert=0;
    for(int i(0); i<aWaterTile.size(); ++i)
    {
        calcTexCoordBox(aWaterTile[i]);
        SSpecificQuad quad;
        int vrtX = i%16*2;
        int vrtY = int(i/16*2);
        for(vertIndexQuad = 0; vertIndexQuad < 16 ; ++vertIndexQuad)
        {
            buildGeom(aWaterVertex, m_arrWaterVrtData[idVert], vrtX, vrtY);
            quad.addVertex(&m_arrWaterVrtData[idVert]);
            ++idVert;
        }
        quad.rotate(aWaterTile[i].m_rotation);
    }
}

void CSector::generateVertexDataFromTile()
{
    uint vertSize = m_arrLand.size() * m_arrLand.front().size()*16; // 9 vertices, 16 indices per tile
    //m_aVertexData.clear(); // to recalc textures ?
    bool bWater = !m_arrWater.isEmpty();
    m_arrLandVrtData.resize(vertSize); // todo: check size. vrt size == tile count * 9 (verts per tile)
    if(bWater)
        m_arrWaterVrtData.resize(vertSize);
    int curIndex(0), waterIndex(0);
    for(int row(0); row<m_arrLand.size(); ++row)
        for(int col(0); col<m_arrLand[row].size(); ++col)
        {
            m_arrLand[row][col].generateDrawVertexData(m_arrLandVrtData, curIndex);
            if(bWater)
                m_arrWater[row][col].generateDrawVertexData(m_arrWaterVrtData, waterIndex);
        }
}

void CSector::updatePosition()
{
    m_modelMatrix.translate(QVector3D(m_index.x*32.0f, m_index.y*32.0f, .0f));
    m_vertexBuf.bind();
    m_vertexBuf.allocate(m_arrLandVrtData.data(), m_arrLandVrtData.count()*int(sizeof(SVertexData)));
    m_vertexBuf.release();

    QVector<ushort> aInd;
    ushort indOffset = 0;
    QVector<ushort> arrTileInd{0,1,4,3, 1,2,5,4, 3,4,7,6, 4,5,8,7}; // indices of quad vertices
    for(int i(0); i<256; ++i) // 256 - tile number per sector
    {
        for(auto ind: arrTileInd)
        {
            aInd.append(ind += indOffset);
        }
        indOffset += 9; // vertices per tile.
    }

    m_indexBuf.bind();
    m_indexBuf.allocate(aInd.data(), aInd.count() * int(sizeof(ushort)));
    m_indexBuf.release();

    //water section
    if(!m_arrWaterVrtData.isEmpty())
    {
        //m_modelMatrix.translate(QVector3D(m_index.x*32.0f, m_index.y*32.0f, .0f));
        m_waterVertexBuf.bind();
        m_waterVertexBuf.allocate(m_arrWaterVrtData.data(), m_arrWaterVrtData.count()*int(sizeof(SVertexData)));
        m_waterVertexBuf.release();

        m_waterIndexBuf.bind();
        m_waterIndexBuf.allocate(aInd.data(), aInd.count() * int(sizeof(ushort)));
        m_waterIndexBuf.release();
    }
}

bool CSector::pickTile(int& outRow, int& outCol, QVector3D& point, bool bLand)
{
    if(!bLand && m_arrWater.isEmpty())
        return false;

    // convert pos to local coords
    point.setX(point.x()-m_index.x*32.0f);
    point.setY(point.y()-m_index.y*32.0f);

    if(bLand)
    {
        for(int row(0); row<m_arrLand.size(); ++row)
            for(int col(0); col<m_arrLand[row].size(); ++col)
            {
                if(m_arrLand[row][col].isProjectTile(point))
                {
                    outRow = row;
                    outCol = col;
                    return true;
                }
            }
    }
    else
    {
        for(int row(0); row<m_arrWater.size(); ++row)
            for(int col(0); col<m_arrWater[row].size(); ++col)
            {
                if(m_arrWater[row][col].isProjectPoint(point))
                {
                    outRow = row;
                    outCol = col;
                    return true;
                }
            }
    }
    return false;
}


void CSector::draw(QOpenGLShaderProgram* program)
{

    if (m_arrLandVrtData.count() == 0)
        return;

    program->setUniformValue("u_modelMmatrix", m_modelMatrix);

    int offset = 0;
    m_vertexBuf.bind();
    int vertLoc = program->attributeLocation("a_position");
    program->enableAttributeArray(vertLoc);
    program->setAttributeBuffer(vertLoc, GL_FLOAT, offset, 3, int(sizeof(SVertexData)));

    offset += int(sizeof(QVector3D));
    int normLoc = program->attributeLocation("a_normal");
    program->enableAttributeArray(normLoc);
    program->setAttributeBuffer(normLoc, GL_FLOAT, offset, 3, int(sizeof(SVertexData)));

    offset+=int(sizeof(QVector3D)); // size of normal
    int textureLocation = program->attributeLocation("a_texture");
    program->enableAttributeArray(textureLocation);
    program->setAttributeBuffer(textureLocation, GL_FLOAT, offset, 2, int(sizeof(SVertexData)));

    m_indexBuf.bind();
    constexpr int indexNum = 256*16; // numTiles * index per tile
    glDrawElements(GL_QUADS, indexNum, GL_UNSIGNED_SHORT, nullptr);

}

void CSector::drawWater(QOpenGLShaderProgram *program)
{
    if (m_arrWaterVrtData.count() == 0)
        return;

    program->setUniformValue("u_modelMmatrix", m_modelMatrix);

    int offset = 0;
    m_waterVertexBuf.bind();
    int vertLoc = program->attributeLocation("a_position");
    program->enableAttributeArray(vertLoc);
    program->setAttributeBuffer(vertLoc, GL_FLOAT, offset, 3, int(sizeof(SVertexData)));

    offset += int(sizeof(QVector3D));
    int normLoc = program->attributeLocation("a_normal");
    program->enableAttributeArray(normLoc);
    program->setAttributeBuffer(normLoc, GL_FLOAT, offset, 3, int(sizeof(SVertexData)));

    offset+=int(sizeof(QVector3D)); // size of normal
    int textureLocation = program->attributeLocation("a_texture");
    program->enableAttributeArray(textureLocation);
    program->setAttributeBuffer(textureLocation, GL_FLOAT, offset, 2, int(sizeof(SVertexData)));

    m_waterIndexBuf.bind();
    glDrawElements(GL_QUADS, m_arrWaterVrtData.count(), GL_UNSIGNED_SHORT, nullptr);
}

bool CSector::projectPt(QVector3D& point)
{
    QVector3D origin(point.x()-m_index.x*32.0f, point.y()-m_index.y*32.0f, 0.0f); // point in sector local coords

    int approxDeviation = 2;
    int xApprox = origin.x()/2;
    int xMin = qMax(0, xApprox-approxDeviation);
    int xMax = qMin(16, xApprox+approxDeviation);

    int yApprox = origin.y()/2;
    int yMin = qMax(0, yApprox-approxDeviation);
    int yMax = qMin(16, yApprox+approxDeviation);

    for(int row(yMin); row<yMax; ++row)
        for(int col(xMin); col<xMax; ++col)
        {
            if(m_arrLand[row][col].isProjectPoint(origin))
            {
                point.setZ(point.z() + origin.z());
                return true;
            }
        }

    return false;
}

void CSector::setTile(QVector3D& point, int index, int rotNum, bool bLand, int matIndex)
{
    int row, col;

    if(!pickTile(row, col, point, bLand))
        return;

    if(bLand)
        m_arrLand[row][col].setTile(index, rotNum);
    else
    {
        m_arrWater[row][col].setTile(index, rotNum);
        m_arrWater[row][col].setMaterialIndex(short(matIndex));
    }
    generateVertexDataFromTile(); //todo: apply changes locally, stop re-generating all data
    m_modelMatrix.setToIdentity(); // todo
    updatePosition(); //todo
}

//void CSector::setTile(const STileLocation tileLoc, const STileInfo tileInfo)
//{
//    CTile* pTile = nullptr;
//    if(tileLoc.bLand)
//        pTile = &(m_arrLand[tileLoc.row][tileLoc.col]);
//    else
//    {
//        pTile = &(m_arrWater[tileLoc.row][tileLoc.col]);
//        pTile->setMaterialIndex(tileInfo.matIndex);
//    }
//    pTile->setTile(tileInfo.index, tileInfo.rotNum);
//    updateDrawData();
//}

void CSector::setTile(const QMap<STileLocation, STileInfo>& arrTileInfo)
{
    CTile* pTile = nullptr;
    for(auto& tile: arrTileInfo.toStdMap())
    {
        if(tile.first.bLand)
            pTile = &(m_arrLand[tile.first.row][tile.first.col]);
        else
        {
            pTile = &(m_arrWater[tile.first.row][tile.first.col]);
            pTile->setMaterialIndex(tile.second.matIndex);
        }
        pTile->setTile(tile.second.index, tile.second.rotNum);
    }
    updateDrawData();
}

bool CSector::existsTileIndices(const QVector<int>& arrInd)
{
    bool bRes = false;
    for(int row(0); row<m_arrLand.size(); ++row)
        for(int col(0); col<m_arrLand[row].size(); ++col)
        {
            if(arrInd.contains(m_arrLand[row][col].tileIndex()))
            {
                qDebug() << "tile row: " << row << " col: " << col << " has invalid index of tile:" << m_arrLand[row][col].tileIndex();
                bRes = true;
            }
        }
    return bRes;
}

void CSector::updateDrawData()
{
    generateVertexDataFromTile(); //todo: apply changes locally, stop re-generating all data
    m_modelMatrix.setToIdentity(); // todo
    updatePosition(); //todo
}

STile::STile(ushort packedData)
{
    m_index = packedData & 63; //first 6 bits
    m_texture = (packedData >> 6) & 255; // second 8 bits
    m_rotation = (packedData >> 14) & 3; // 2 bits more
}

ushort STile::packData(ushort m_index, ushort m_texture, ushort m_rotation)
{

    return (m_index & 63) | ((m_texture & 255) << 6) | ((m_rotation & 3) << 14);
}
