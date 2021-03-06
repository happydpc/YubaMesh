//
// Created by user on 2018/8/17.
//

#ifndef MESHDEVELOP_HALFEDGE_H
#define MESHDEVELOP_HALFEDGE_H

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>
#include <map>
#include "type_mesh.h"

namespace YbMesh {


struct IHalfEdge;

struct Vert
{
    int edge;
    glm::vec3 v;
};
typedef std::vector<Vert>::iterator VertIt;

typedef YbMesh::indicesTriMesh<glm::vec3> IMesh;
typedef std::vector<IHalfEdge>::iterator IHalfEdgeIt;

class SharedHalfEdge {
public:
    SharedHalfEdge(IMesh& imesh);
    static SharedHalfEdge* ptr;

    std::vector<IHalfEdge> edges;
    std::vector<Vert> verts;

    std::vector<int> findex;

    IHalfEdgeIt edge(int f);

    IMesh toMesh();

private:

    friend struct  IHalfEdge;
};

struct IHalfEdge{
    IHalfEdge():_pair(-1){}
    IHalfEdge(int from, int to):_pair(-1),_to(to),_from(from){}
    IHalfEdgeIt next();
    IHalfEdgeIt pair();
    VertIt to();
    VertIt from();
    glm::vec3 vector();
    int _next, _pair, _face, _to;

private:
    int _from; // mostly used for caclulate _pair
    friend class SharedHalfEdge;
};



}

#endif //MESHDEVELOP_HALFEDGE_H
