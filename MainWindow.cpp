#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/basic/graph_generators.h>
#include <ogdf/layered/DfsAcyclicSubgraph.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/layered/SugiyamaLayout.h>
#include <ogdf/layered/OptimalRanking.h>
#include <ogdf/layered/MedianHeuristic.h>
#include <ogdf/layered/OptimalHierarchyLayout.h>

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QMessageBox>
#include <QGraphicsProxyWidget>
#include <QPaintEvent>
#include <QDebug>

#include <memory>
#include <vector>

template <typename T>
class Node
{
public:
    explicit Node(ogdf::Graph* G, ogdf::GraphAttributes* GA, ogdf::node ogdfNode, T data)
    {
        this->_G = G;
        this->_GA = GA;
        this->_ogdfNode = ogdfNode;
        this->_data = data;
    }

    Node<T>* setLeft(Node* left)
    {
        return left ? this->_left = makeNodeWithEdge(left) : nullptr;
    }

    Node<T>* setRight(Node* right)
    {
        return right ? this->_right = makeNodeWithEdge(right) : nullptr;
    }

    Node<T>* setData(T data)
    {
        this->_data = data;
        return this;
    }

    Node<T>* left()
    {
        return this->_left;
    }

    Node<T>* right()
    {
        return this->_right;
    }

    T data()
    {
        return this->_data;
    }

    ogdf::node ogdfNode()
    {
        return this->_ogdfNode;
    }

private:
    Node<T>* _left;
    Node<T>* _right;
    T _data;
    ogdf::Graph* _G;
    ogdf::GraphAttributes* _GA;
    ogdf::node _ogdfNode;

    Node* makeNodeWithEdge(Node<T>* node)
    {
        auto edge = this->_G->newEdge(this->_ogdfNode, node->_ogdfNode);
        this->_GA->arrowType(edge) = ogdf::EdgeArrow::eaLast;
        return node;
    }
};

template <typename T>
class Tree
{
public:
    explicit Tree(ogdf::Graph* G, ogdf::GraphAttributes* GA)
    {
        this->_G = G;
        this->_GA = GA;
    }

    Node<T>* newNode(T data)
    {
        auto ogdfNode = _G->newNode();
        auto node = new Node<T>(_G, _GA, ogdfNode, data);
        _ogdfDataMap[ogdfNode] = node;
        _nodePool.push_back(std::unique_ptr<Node<T>>(node));
        return node;
    }

    Node<T>* findNode(ogdf::node ogdfNode)
    {
        auto found = _ogdfDataMap.find(ogdfNode);
        return found != _ogdfDataMap.end() ? found->second : nullptr;
    }

private:
    ogdf::Graph* _G;
    ogdf::GraphAttributes* _GA;
    std::vector<std::unique_ptr<Node<T>>> _nodePool;
    std::map<ogdf::node, Node<T>*> _ogdfDataMap;
};

class GraphNode : public QWidget
{
public:
    GraphNode()
    {
    }

    GraphNode(QString label)
    {
        setLabel(label);
        this->setStyleSheet("border: 1px solid blue");
        this->setContentsMargins(0,0,0,0);

    }

    GraphNode(const GraphNode & other)
    {
        setLabel(other._label);
    }

    GraphNode & operator=(const GraphNode & other)
    {
        setLabel(other._label);
        return *this;
    }

    QRectF boundingRect() const
    {
        return QRectF(0, 0, _cachedWidth, _cachedHeight);
    }

    void paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        QPainter painter(this);

        painter.save();

        //draw bounding rectangle
        QRectF rect = boundingRect();
        painter.setPen(Qt::red);
        //painter.drawRect(rect);

        //draw node contents
        painter.setPen(Qt::black);
        painter.setFont(this->_font);
        QRect textRect = QRect(_spacingX, _spacingY, rect.width() - _spacingX, rect.height() - _spacingY);
        painter.drawText(textRect, _label);

        painter.restore();
    }

    void mousePressEvent(QMouseEvent* event)
    {
        Q_UNUSED(event);

        QMessageBox::information(nullptr, "clicked", _label);
    }

    void setLabel(const QString & label)
    {
        _label = label;
        updateCache();
    }

    void updateCache()
    {
        QFontMetrics metrics(this->_font);
        _cachedWidth = metrics.width(this->_label) + _spacingX * 2;
        _cachedHeight = metrics.height() + _spacingY * 2;
    }

    QString label()
    {
        return _label;
    }

private:
    QString _label;
    QFont _font = QFont("Lucida Console", 8, QFont::Normal, false);
    const qreal _spacingX = 3;
    const qreal _spacingY = 3;
    qreal _cachedWidth;
    qreal _cachedHeight;
};
#include <QPainterPath>
class GraphEdge : public QAbstractGraphicsShapeItem
{
public:
    GraphEdge(QPointF start, QPointF end, ogdf::DPolyline bends, QRectF sourceRect, QRectF targetRect) : QAbstractGraphicsShapeItem()
    {
        QList<QPointF> points;
        points << start;
        for(auto p : bends)
            points << QPointF(p.m_x, p.m_y);
        points << end;

        QPointF nearestI = calculateNearestIntersect(sourceRect, points[0], points[1]);
        qDebug() << "nearestI" << nearestI;
        points[0]=nearestI;
        int len = points.length();
        nearestI = calculateNearestIntersect(targetRect, points[len-1], points[len-2]);
        points[len-1]=nearestI;

        preparePainterPath(points);
    }

    QRectF boundingRect() const
    {
        return QRectF(0, 0, 0, 0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        painter->setRenderHint(QPainter::Antialiasing);

        painter->save();

        int lineSize = 2;

        painter->setPen(QPen(Qt::green, lineSize));
        painter->drawRect(boundingRect());
        painter->drawPath(_line);

        painter->setPen(QPen(Qt::green, lineSize));
        painter->drawPath(_arrow);

        painter->restore();
    }

    qreal calculateDistance(QPointF p1, QPointF p2)
    {
        QPointF d = p2 - p1;
        return sqrt(d.x() * d.x() + d.y() * d.y());
    }

    QPointF calculateNearestIntersect(QRectF rect, QPointF p1, QPointF p2)
    {
        qDebug() << "calculateNearest";
        qDebug() << "rect" << rect.topLeft() << rect.bottomRight();
        qDebug() << "p1" << p1;
        qDebug() << "p2" << p2;
        //y=a*x+b
        //a = dy/dx = (p1.y-p2.y)/(p1.x-p2.x)
        //b = p1.y - p1.x;

        qreal div = p1.x()-p2.x();

        if(div == 0)
        {
            QPointF i1(p1.x(), rect.top());
            qDebug() << "i1" << i1;
            QPointF i2(p1.x(), rect.bottom());
            qDebug() << "i2" << i2;

            if(p2.y() < p1.y())
                return i1;
            else
                return i2;
        }
        else
        {
            QPointF result;
            qreal bestDist = 10e99;

            qreal a = (p1.y()-p2.y()) / div;
            qreal b = p1.y() - a * p1.x();
            qDebug() << "a" << a << "b" << b;

            //intersect 1
            //rect.top() = a*x+b;
            //x = (b - rect.top()) / -a
            QPointF i1((b - rect.top()) / -a, rect.top());
            qDebug() << "i1" << i1;
            qDebug() << "consider?" << rect.contains(i1);
            if(rect.contains(i1))
            {
                qreal dist = calculateDistance(p2, i1);
                if(dist < bestDist)
                {
                    bestDist = dist;
                    result = i1;
                }
            }

            //intersect 2
            //rect.bottom() = a*x+b
            //x = (b - rect.bottom()) / -a
            QPointF i2((b - rect.bottom()) / -a, rect.bottom());
            qDebug() << "i2" << i2;
            qDebug() << "consider?" << rect.contains(i2);
            if(rect.contains(i2))
            {
                qreal dist = calculateDistance(p2, i2);
                if(dist < bestDist)
                {
                    bestDist = dist;
                    result = i2;
                }
            }

            //intersect 3
            //x=rect.left()
            QPointF i3(rect.left(), a * rect.left() + b);
            qDebug() << "i3" << i3;
            qDebug() << "consider?" << rect.contains(i3);
            if(rect.contains(i3))
            {
                qreal dist = calculateDistance(p2, i3);
                if(dist < bestDist)
                {
                    bestDist = dist;
                    result = i3;
                }
            }

            //intersect 4
            //x=rect.right()
            QPointF i4(rect.right(), a * rect.right() + b);
            qDebug() << "i4" << i4;
            qDebug() << "consider?" << rect.contains(i4);
            if(rect.contains(i4))
            {
                qreal dist = calculateDistance(p2, i4);
                if(dist < bestDist)
                {
                    bestDist = dist;
                    result = i4;
                }
            }
            return result;
        }
        qDebug() << " ";
    }

    void preparePainterPath(QList<QPointF> points)
    {
        //edge line
        QPolygonF polyLine;
        for(auto p : points)
            polyLine << p;
        _line.addPolygon(polyLine);

        //arrow
        int len=points.length();
        QLineF perpLine = QLineF(points[len-1], points[len-2]).normalVector();

        qreal arrowLen = 6;

        QLineF a;
        a.setP1(points[len-1]);
        a.setAngle(perpLine.angle() - 45);
        a.setLength(arrowLen);

        QLineF b;
        b.setP1(points[len-1]);
        b.setAngle(perpLine.angle() - 135);
        b.setLength(arrowLen);

        QLineF c;
        c.setP1(a.p2());
        c.setP2(b.p2());

        QPolygonF arrowLine;
        arrowLine << a.p1() << a.p2() << b.p1() << b.p2() << c.p1() << c.p2();
        _arrow.addPolygon(arrowLine);
    }

private:
    QPainterPath _line;
    QPainterPath _arrow;
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->doStuff();
}

void MainWindow::doStuff()
{
    using namespace ogdf;

    //initialize graph
    Graph G;
    GraphAttributes GA(G, GraphAttributes::nodeGraphics |
                       GraphAttributes::edgeGraphics |
                       GraphAttributes::nodeLabel |
                       GraphAttributes::nodeStyle |
                       GraphAttributes::edgeType |
                       GraphAttributes::edgeArrow |
                       GraphAttributes::edgeStyle);

    //add nodes
    Tree<GraphNode*> tree(&G, &GA);
    auto root = tree.newNode(new GraphNode("rp"));
    auto left = root->setLeft(tree.newNode(new GraphNode("left 1")));
    left->setLeft(tree.newNode(new GraphNode("left 2")));
    left->setRight(tree.newNode(new GraphNode("right 2")));
    auto right = root->setRight(tree.newNode(new GraphNode("right 1")));
    right->setLeft(tree.newNode(new GraphNode("left 3")))->setRight(left);
    right->setRight(tree.newNode(new GraphNode("right 3")))->setRight(tree.newNode(new GraphNode("nice long text :)")))->setLeft(root);

    //adjust node size
    node v;
    forall_nodes(v, G)
    {
        auto node = tree.findNode(v);
        if (node)
        {
            auto rect = node->data()->boundingRect();
            GA.width(v) = rect.width();
            GA.height(v) = rect.height();
        }
    }

    //do layout
    ogdf::OptimalHierarchyLayout* OHL = new ogdf::OptimalHierarchyLayout;
    OHL->nodeDistance(25.0);
    OHL->layerDistance(50.0);
    OHL->weightBalancing(0.0);
    OHL->weightSegments(0.0);

    SugiyamaLayout SL;
    SL.setRanking(new OptimalRanking);
    SL.setCrossMin(new MedianHeuristic);
    SL.alignSiblings(false);
    SL.setLayout(OHL);
    SL.call(GA);

    QGraphicsScene* scene = new QGraphicsScene(this);

    //draw widget contents (nodes)
    forall_nodes(v, G)
    {
        auto node = tree.findNode(v);
        if (node)
        {
            //draw node using x,y
            auto rect = node->data()->boundingRect();
            qreal x = GA.x(v) - (rect.width()/2);
            qreal y = GA.y(v) - (rect.height()/2);
            node->data()->setGeometry(x, y, rect.width(), rect.height());
            scene->addWidget(node->data());
        }
    }

    //draw edges
    edge e;
    forall_edges(e, G)
    {
        const auto bends = GA.bends(e);
        const auto source = e->source();
        const auto target = e->target();

        GraphNode* sourceGraphNode = tree.findNode(source)->data();
        GraphNode* targetGraphNode = tree.findNode(target)->data();
        qDebug() << "edge" << sourceGraphNode->label() << "->" << targetGraphNode->label();

        QRectF sourceRect = sourceGraphNode->geometry();
        sourceRect.adjust(-4, -4, 4, 4);
        QRectF targetRect = targetGraphNode->geometry();
        targetRect.adjust(-4, -4, 4, 4);

        QPointF start(GA.x(source), GA.y(source));
        QPointF end(GA.x(target), GA.y(target));
        GraphEdge* edge = new GraphEdge(start, end, bends, sourceRect, targetRect);

        qDebug() << start;
        for(auto p : bends)
        {
            qDebug() << QPointF(p.m_x, p.m_y);
        }
        qDebug() << end;
        scene->addItem(edge);
    }

    //scene->setSceneRect(0, 0, GA.boundingBox().width(), GA.boundingBox().height());

    //draw scene
    scene->setBackgroundBrush(QBrush(Qt::darkGray));
    ui->graphicsView->setScene(scene);
    ui->graphicsView->show();

    GraphIO::drawSVG(GA, "test.svg");
}

MainWindow::~MainWindow()
{
    delete ui;
}
