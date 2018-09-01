#define PLUGINBACKEND_CPP
#include "PluginBackend.h"
#define PluginBackend_CPP
#include "PluginBackend.h"
#include <YbCore/controller>
#include <YbCore/coretool>
#include <glm/glm.hpp>
#include <QStandardPaths>
#include <glm/gtc/matrix_transform.hpp>
#include "./algorithm/MeshProcessing.h"
#include <YbMesh/YbMesh.hpp>
#include "SliceRender.h"
PluginBackend::PluginBackend(IPluginBackend* parent)
{

}

void PluginBackend::workMode(QString mode) {
    if(mode == "scan")
        render_s->changeRender(std::bind(&SliceRender::scanLineAnimation, render_s, std::placeholders::_1));
    else if(mode == "pick")
        render_s->changeRender(std::bind(&SliceRender::drawModelWithSlice, render_s, std::placeholders::_1));
    con<RenderCtrl>().update();
}

void PluginBackend::construction() {
    QString shader_prefix = PLUGINPATH"Measurement/shaders/";
    RenderScript([=](QTime& t) {
        con<ShaderCtrl>().addShaderProgram("base", shaderConfig{ V(shader_prefix+"indices"),F(shader_prefix+"indices") });
    });
    render_s = new("base") SliceRender();
    this->importMesh(MESHPATH"body2.obj","scanbody");
}

void PluginBackend::destruction() {

}

bool PluginBackend::importMesh(std::string url,std::string name){
    YbMesh::indicesTriMesh<glm::vec3> triMesh = YbMesh::IO::importOBJ_V0(url);
    auto object = new InteractiveObject(triMesh,YbMesh::indicesTriMesh<glm::vec3>(std::make_shared<std::vector<glm::vec3>>(),triMesh.f()));
    object->centerlized();
    object->calculateNorm();
    glm::mat3 pca   = YbMesh::slice::pca_analysic(triMesh,2);
    glm::mat4 coord = glm::rotate(glm::mat4(),-3.1415926f*0.5f,glm::vec3(0,0,1))* glm::mat4(glm::transpose(pca));
    object->model = coord * object->model;
    norm = pca[2];
    YbMesh::slice::sortByVector(triMesh,norm);
    RenderScript([=](QTime&) {
        object->createBufferScript();
        object->syncVertexBuffersDataScript();
        object->syncFacesBuffersDataScript();
        object->syncSelectBufferScript();
    });
    con<InteractiveCtrl>().addInteractiveObject(name, object);

    auto& v = triMesh.v();
    auto& f = triMesh.f();
    render_s->bounding_z[0] = glm::dot(norm,v[(*f.begin())[0]]);
    render_s->bounding_z[1] = glm::dot(norm,v[(*std::prev(f.end()))[0]]);

    float model_height = glm::dot(norm,(v[(*f.begin())[0]]-v[(*std::prev(f.end()))[0]])*glm::mat3(coord));
    emit heightUpdate(QString::number(model_height,'f',2));
    for(int i=0; i < f.size(); i += 10) {
        gap = std::max(gap, abs(glm::dot(norm,v[f[i][0]]-v[f[i][1]]))*20);
    }

//    QObject::connect(object,&PickableMesh::Selected, object, [=](PickableMesh* mesh){
//        auto select_file = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString()+"/selections.obj";
//        plugin::writePartialMesh(mesh, con<CentralCtrl>().selectTool->getSelectedFace(), select_file);
////        auto border_file = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString()+"/borders.obj";
////        auto border_face = plugin::extractMeshBorder(mesh, con<CentralCtrl>().selectTool->getSelectedFace());
////        plugin::writePartialMesh(mesh, border_face, border_file);
//    });
    return true;
}

void PluginBackend::slice(float dis) {
    auto mesh = con<InteractiveCtrl>().object("scanbody")->m_v;
    auto interval = YbMesh::slice::getSliceInterval(mesh, norm, dis, gap);
    std::vector<std::array<glm::vec3,2>> slice_res;
    for(auto it = interval[0]; it != interval[1]; it++) {
        if(YbMesh::slice::isFaceInersected(mesh, *it, norm, dis)) {
            slice_res.emplace_back(YbMesh::slice::getFaceIntersection(mesh, *it, norm, dis));
        }
    }
    auto view = con<ViewCtrl>().view();
    auto mvp  = view->matrixVP()*view->model()* glm::rotate(glm::mat4(),-3.1415926f*0.5f,glm::vec3(1,0,0))*con<InteractiveCtrl>().object("scanbody")->model;
    float w = canvas->width();
    float h = canvas->height();
    std::vector<float> girths;

    contours.clear();
    auto& v = mesh.v();
    for(auto& intervals: plugin::sortContours(slice_res)) {
        contours.emplace_back(std::vector<QPoint>());
        girths.emplace_back(0);
        for(auto it = intervals[0]; it != intervals[1]; it++) {
            glm::vec3 p = (glm::vec3(mvp*glm::vec4(v[(*it)[0][0]]+(*it)[0][2]*(v[(*it)[0][1]] - v[(*it)[0][0]]),1.0))+1.0f)*0.5f;
            std::prev(contours.end())->emplace_back(QPoint(p[0]*w,(1-p[1])*h));
            *std::prev(girths.end()) += glm::length(v[(*it)[0][0]]+(*it)[0][2]*(v[(*it)[0][1]] - v[(*it)[0][0]])-
                                                    v[(*it)[1][0]]-(*it)[1][2]*(v[(*it)[1][1]] - v[(*it)[1][0]]));
        }
    }

    emit sliceUpdate(QString::number(*std::max_element(girths.begin(),girths.end()),'f',2));

//    plugin::writeSlices(mesh, slice_res, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString()+"/slice.obj");

    emit canvas->sliceChanged();
}

void PluginBackend::slice(int x, int y) {
    int f_id = con<InteractiveCtrl>().pickTool->getSingleFacePick(x,y);
    if(f_id == -1) {
         canvas->update();
         return ;
    }
    auto mesh = con<InteractiveCtrl>().object("scanbody");
    auto& v = mesh->m_v.v();
    auto& f = mesh->m_v.f();
    render_s->dis = glm::dot(norm, (v[f[f_id][0]]+v[f[f_id][1]]+v[f[f_id][2]])/3.0f);
    slice(render_s->dis);
    con<RenderCtrl>().update();
}

void PluginBackend::setSliceCanvas(QQuickPaintedItem* item) {
    canvas = (SlicesCanvas*)item;
}

#include <unsupported/Eigen/FFT>

void PluginBackend::test(std::vector<std::complex<float> >&  timevec) {
    static  int freq = 15;
    Eigen::FFT<float> fft;

    std::vector<std::complex<float> > freqvec;

    fft.fwd( freqvec,timevec);

    if(freqvec.size() > freq * 5)
        std::fill(freqvec.begin()+std::min((int)timevec.size(), freq),  freqvec.end()-std::min((int)timevec.size(), freq), std::complex<float>(0,0));

    fft.inv( timevec,freqvec);
}

template <>
QObject* Backend<PluginBackend>(QQmlEngine *pQMLEngine,QJSEngine *pJSEngine) {
    Q_UNUSED(pQMLEngine);
    Q_UNUSED(pJSEngine);
    return ICtrl<PluginBackend>::getInstance();
}

//#include "moc_PluginBackend.cpp"
