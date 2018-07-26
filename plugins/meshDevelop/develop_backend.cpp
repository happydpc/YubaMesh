#include "develop_backend.h"
#include <functional>
#include <easylog/easylogging++.h>
#include <controller>
#include <mesh_proc>

#include <QStandardPaths>
#include <fstream>
// OPENCV

INITIALIZE_EASYLOGGINGPP
develop_backend::develop_backend(QObject* parent)
    : QObject(parent)
{
}

void develop_backend::instance() {
#if defined(Q_OS_OSX)
    QString prefix = "../PlugIns/MeshDevelop/shaders/";
#elif defined(Q_OS_WIN)
    QString prefix = "./MeshDevelop/shaders/";
#endif
    RenderScript([=](QTime &t) mutable {
        con<ShaderCtrl>().addShaderProgram("texture", shaderConfig{ V(prefix+"texture"),G(prefix+"texture"),F(prefix+"texture") });
        con<ShaderCtrl>().addShaderProgram("base", shaderConfig{ V(prefix+"indices"),F(prefix+"indices") });
        PickableMesh* mesh = readObj("../PlugIns/GLViewer/mesh/body_sample.obj");
        calculateNorm(mesh);
        centerlized(mesh);
        sortByVector(mesh, glm::normalize(glm::mat3(glm::inverse(mesh->model))*glm::vec3(0,1,0)));
        mesh->createBuffers();
        mesh->syncVertexBuffersData();
        mesh->syncFacesBuffersData();
        con<MeshCtrl>().add_mesh("scanbody",mesh); //bunny FullBodyScan 20180205142827.cie
    });
    render_a = new("render") A;
}

glm::vec3 orientN = glm::vec3(0,1,0);

A::A():RenderScript(std::bind(&A::scan_line_animation,this,std::placeholders::_1)){
    RenderScript([this](QTime& t){
        auto mesh = con<MeshCtrl>().mesh("scanbody");
//        orientN = glm::normalize(glm::mat3(glm::inverse(mesh->model))*glm::vec3(0,1,0));
        this->dot_v = glm::vec2(glm::dot(orientN, mesh->v[(*mesh->f.begin())[0]].mv), glm::dot(orientN, mesh->v[(*mesh->f.end())[0]].mv));
//        auto he = halfEdge(mesh, mesh->f.begin(), mesh->f.end());

//        std::fstream fileHandle;
//        fileHandle.open(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString()+"/a.obj",std::ifstream::out);
////        for(float percent = 0.1; percent < 0.9; percent += 0.02)
//        float percent = 1-0.72;
//        for(auto line:he.get_slice(orientN, this->dot_v[0]+(this->dot_v[1]-this->dot_v[0])*percent,(this->dot_v[1]-this->dot_v[0])*0.05)) {
//            fileHandle << "v " << line[0][0] << ' ' << line[0][1] << ' ' << line[0][2] << std::endl;
//            fileHandle << "v " << line[1][0] << ' ' << line[1][1] << ' ' << line[1][2] << std::endl;
//        }
//        fileHandle.close();
    });
}

void A::scan_line_animation(QTime& t) {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto shader = con<ShaderCtrl>().shader("base");
    auto mesh = con<MeshCtrl>().mesh("scanbody");
    auto view = con<ViewCtrl>().view();

    shader->bind();
    shader->setUniformValue("camera_vp", view->MatrixVP());
    shader->setUniformValue("model", view->Model()*mesh->Model());
    auto v = glm::vec4(orientN,dot_v[0]+(dot_v[1]-dot_v[0])*percent);
    shader->setUniformValue("orient", v[0],v[1],v[2],v[3]);
    auto p = std::lower_bound(mesh->f.begin(),mesh->f.end(), dot_v[0]+(dot_v[1]-dot_v[0])*percent,
            [=](glm::ivec3& e1,float v){
        return v < glm::dot(mesh->v[e1[0]].mv, orientN);
    });

    mesh->drawElements(0, (p - mesh->f.begin())); //
    shader->release();

    percent += 1e-3;
    percent = percent > 1.0 ? 0: percent;
    con<RenderCtrl>().update(true);

    LOG(INFO) << "percent: " << percent;

//    glEnable(GL_DEPTH_TEST);
//    glClearColor(0.2, 0.3, 0.3, 1.0);
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//    auto shader = con<ShaderCtrl>().shader("core",true);
//    auto mesh = con<MeshCtrl>().mesh("scanbody");
//    auto view = con<ViewCtrl>().view();
//    if(!shader || !mesh || !view) return;

//    shader->bind();
//    shader->setUniformValue("camera_vp", view->MatrixVP());
//    shader->setUniformValue("model", view->Model()*mesh->Model());

//    mesh->drawElements();

//    shader->release();
}

void A::draw_model(QTime &t) {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    auto shader = con<ShaderCtrl>().shader("base");
    auto mesh = con<MeshCtrl>().mesh("scanbody");
    auto view = con<ViewCtrl>().view();

    shader->bind();
    shader->setUniformValue("camera_vp", view->MatrixVP());
    shader->setUniformValue("model", view->Model()*mesh->Model());
    auto p1 = std::lower_bound(mesh->f.begin(),mesh->f.end(), dot_v[0]+(dot_v[1]-dot_v[0])*(percent-0.01),
            [=](glm::ivec3& e1,float v){
        return v < glm::dot(mesh->v[e1[0]].mv, glm::mat3(glm::inverse(mesh->model))*glm::vec3(0,1,0));
    });

    auto p2 = std::lower_bound(mesh->f.begin(),mesh->f.end(), dot_v[0]+(dot_v[1]-dot_v[0])*(percent+0.01),
            [=](glm::ivec3& e1,float v){
        return v < glm::dot(mesh->v[e1[0]].mv, glm::mat3(glm::inverse(mesh->model))*glm::vec3(0,1,0));
    });

    mesh->drawElements( (p1 - mesh->f.begin()), (p2 - p1));

    shader->release();
}

void develop_backend::draw_for(QString value) {
    if(value == "mesh") {
        render_a->set_render(std::bind(&A::draw_model,render_a,std::placeholders::_1));
    }
    //    new ("texMix") RenderScript([=](RenderScript* rf){
    //        TGL.glEnable(GL_DEPTH_TEST);
    //        TGL.glClearColor(0.2, 0.3, 0.3, 1.0);
    //        TGL.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //        auto shader = SMM("texture");
    //        auto mesh = TMM("bunny");
    //        if(mesh->texture)
    //            mesh->texture->bind();
    //        shader->bind();
    //        mesh->indicesDraw();
    //        shader->release();
    //        LOG(INFO) << "texture rendering. ";
    //    });
}
