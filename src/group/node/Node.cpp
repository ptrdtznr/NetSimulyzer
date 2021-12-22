/*
 * NIST-developed software is provided by NIST as a public service. You may use,
 * copy and distribute copies of the software in any medium, provided that you
 * keep intact this entire notice. You may improve,modify and create derivative
 * works of the software or any portion of the software, and you may copy and
 * distribute such modifications or works. Modified works should carry a notice
 * stating that you changed the software and should note the date and nature of
 * any such change. Please explicitly acknowledge the National Institute of
 * Standards and Technology as the source of the software.
 *
 * NIST-developed software is expressly provided "AS IS." NIST MAKES NO
 * WARRANTY OF ANY KIND, EXPRESS, IMPLIED, IN FACT OR ARISING BY OPERATION OF
 * LAW, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT
 * AND DATA ACCURACY. NIST NEITHER REPRESENTS NOR WARRANTS THAT THE
 * OPERATION OF THE SOFTWARE WILL BE UNINTERRUPTED OR ERROR-FREE, OR THAT
 * ANY DEFECTS WILL BE CORRECTED. NIST DOES NOT WARRANT OR MAKE ANY
 * REPRESENTATIONS REGARDING THE USE OF THE SOFTWARE OR THE RESULTS THEREOF,
 * INCLUDING BUT NOT LIMITED TO THE CORRECTNESS, ACCURACY, RELIABILITY,
 * OR USEFULNESS OF THE SOFTWARE.
 *
 * You are solely responsible for determining the appropriateness of using and
 * distributing the software and you assume all risks associated with its use,
 * including but not limited to the risks and costs of program errors,
 * compliance with applicable laws, damage to or loss of data, programs or
 * equipment, and the unavailability or interruption of operation. This
 * software is not intended to be used in any situation where a failure could
 * cause risk of injury or damage to property. The software developed by NIST
 * employees is not subject to copyright protection within the United States.
 *
 * Author: Evan Black <evan.black@nist.gov>
 */

#include "Node.h"
#include "../../conversion.h"
#include "../../util/undo-events.h"
#include <cmath>
#include <glm/glm.hpp>
#include <utility>

namespace netsimulyzer {

Node::Node(const Model &model, parser::Node ns3Node)
    : model(model), ns3Node(std::move(ns3Node)), offset(toRenderCoordinate(this->ns3Node.offset)) {
  this->model.setPosition(toRenderCoordinate(ns3Node.position) + offset);
  this->model.setRotate(ns3Node.orientation[0], ns3Node.orientation[2], ns3Node.orientation[1]);

  if (ns3Node.height) {
    const auto bounds = model.getBounds();
    auto height = std::abs(bounds.max.y - bounds.min.y);

    this->model.setTargetHeightScale(*ns3Node.height / height);
  }

  this->model.setScale(toRenderArray(ns3Node.scale));

  if (ns3Node.baseColor)
    this->model.setBaseColor(toRenderColor(ns3Node.baseColor.value()));
  if (ns3Node.highlightColor)
    this->model.setHighlightColor(toRenderColor(ns3Node.highlightColor.value()));
}

const Model &Node::getModel() const {
  return model;
}

const parser::Node &Node::getNs3Model() const {
  return ns3Node;
}

bool Node::visible() const {
  return ns3Node.visible;
}

glm::vec3 Node::getCenter() const {
  auto position = model.getPosition();
  const auto &bounds = model.getBounds();
  position.y += ns3Node.height.value_or(bounds.max.y - bounds.min.y) * model.getScale().y / 2.0f;

  return position;
}

void Node::addWiredLink(WiredLink *link) {
  wiredLinks.emplace_back(link);
  link->notifyNodeMoved(ns3Node.id, getCenter());
}

undo::MoveEvent Node::handle(const parser::MoveEvent &e) {
  undo::MoveEvent undo;
  undo.position = model.getPosition();
  undo.event = e;

  auto target = toRenderCoordinate(e.targetPosition) + offset;
  this->model.setPosition(target);

  for (auto link : wiredLinks) {
    link->notifyNodeMoved(ns3Node.id, getCenter());
  }

  return undo;
}

undo::NodeOrientationChangeEvent Node::handle(const parser::NodeOrientationChangeEvent &e) {
  undo::NodeOrientationChangeEvent undo;
  undo.orientation = model.getRotate();
  undo.event = e;

  this->model.setRotate(e.targetOrientation[0], e.targetOrientation[2], -e.targetOrientation[1]);

  return undo;
}

undo::NodeColorChangeEvent Node::handle(const parser::NodeColorChangeEvent &e) {
  undo::NodeColorChangeEvent undo;
  undo.event = e;

  if (e.type == parser::NodeColorChangeEvent::ColorType::Base) {
    undo.originalColor = model.getBaseColor();

    if (!e.targetColor.has_value())
      model.unsetBaseColor();
    else
      model.setBaseColor(toRenderColor(e.targetColor.value()));

  } else { // Highlight
    undo.originalColor = model.getHighlightColor();

    if (!e.targetColor.has_value())
      model.unsetHighlightColor();
    else
      model.setHighlightColor(toRenderColor(e.targetColor.value()));
  }

  return undo;
}

void Node::handle(const undo::MoveEvent &e) {
  model.setPosition(e.position);

  for (auto link : wiredLinks) {
    link->notifyNodeMoved(ns3Node.id, getCenter());
  }
}

void Node::handle(const undo::NodeOrientationChangeEvent &e) {
  model.setRotate(e.orientation[0], e.orientation[2], e.orientation[1]);
}

void Node::handle(const undo::NodeColorChangeEvent &e) {
  if (e.event.type == parser::NodeColorChangeEvent::ColorType::Base) {
    if (e.originalColor.has_value())
      model.setBaseColor(e.originalColor.value());
    else
      model.unsetBaseColor();
  } else { // Highlight
    if (e.originalColor.has_value())
      model.setHighlightColor(e.originalColor.value());
    else
      model.unsetHighlightColor();
  }
}

} // namespace netsimulyzer
