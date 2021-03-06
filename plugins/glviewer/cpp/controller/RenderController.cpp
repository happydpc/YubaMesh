#include "RenderController.h"
#include "ViewController.h"
#include "InteractiveController.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QTime>
#include <QThread>
#include <QImage>
#include "../FboNode.h"
#include "../utils/Pick.h"
#include "../utils/ScriptSamples.h"

template<>
LIBSHARED_EXPORT RenderCtrl& con<RenderCtrl>(){
    return ICtrl<RenderCtrl>::getInstanceRef();
}

void RenderCtrl::addScript(RenderScript&& op) {
    anonymouse_lock.lockForWrite();
    anonymous_script.push_back(op);
    anonymouse_lock.unlock();
    update();
}

void RenderCtrl::addScript(RenderScript* op) {
    named_lock.lockForWrite();
    LOG(INFO)<< "Script(Add): " << con<CentralCtrl>().module()+op->name;
    named_script[con<CentralCtrl>().module()+op->name] = op;
    con<CentralCtrl>().resourceHook(CentralCtrl::RENDER,op->name);
    named_lock.unlock();
    update();
}

RenderCtrl::RenderCtrl(){
    con<CentralCtrl>().initialGLFunctions();
}

QOpenGLFramebufferObject* RenderCtrl::createFramebufferObject(const QSize &size){
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    frame_buffer = new QOpenGLFramebufferObject(size, format);

    con<ViewCtrl>().updateSize(size.width(), size.height());
    con<InteractiveCtrl>().pickTool->sizingScreenBufferScript();
    return  frame_buffer;
}

const QOpenGLFramebufferObject& RenderCtrl::frameBufferObject() const{
    return *frame_buffer;
}

void RenderCtrl::render() {
    auto curr_time = QTime::currentTime();
    ((FboNode*)con<CentralCtrl>().GLNode())->set_fps(curr_time.second());
    QQuickFramebufferObject::Renderer::update();
    if(!this->render_volatile) return ;

    render_lock.lockForWrite();
    YbCore::defaultScript::clear(curr_time);

    anonymouse_lock.lockForRead();
    for(auto& e:this->anonymous_script) {
        e.render(curr_time);
    }
    anonymous_script.clear();
    anonymouse_lock.unlock();


    if(!this->render_volatile) return ;

    named_lock.lockForRead();
    for(auto& e:this->named_script){
        e.second->render(curr_time);
    }
    named_lock.unlock();

    render_lock.unlock();

    this->render_volatile = this->for_next_frame;
}

void RenderCtrl::releaseScript(std::string name) {
    LOG(INFO)<< "Script(Del): " << name;
    delete named_script[name];
    named_script.erase(named_script.find(name));
}

void RenderCtrl::update(bool for_next_frame){
    this->render_volatile = true;
    this->for_next_frame = for_next_frame;
}
