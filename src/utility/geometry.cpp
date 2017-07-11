/*
 * Copyright (C) 2017 Incognito
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "geometry.h"
#include "misc.h"

#include "../core.h"
#include "../main.h"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/variant.hpp>

#include <Eigen/Core>

#include <cmath>

using namespace Utility;

bool Utility::doesLineSegmentIntersectArea(const Eigen::Vector3f &lineSegmentStart, const Eigen::Vector3f &lineSegmentEnd, const Item::SharedArea &area)
{
	Eigen::Vector2f height = Eigen::Vector2f::Zero();
	boost::variant<Polygon2D, Box2D, Box3D, Eigen::Vector2f, Eigen::Vector3f> position;
	if (area->attach)
	{
		height = area->height;
		position = area->attach->position;
	}
	else
	{
		height = area->height;
		position = area->position;
	}
	switch (area->type)
	{
		case STREAMER_AREA_TYPE_CIRCLE:
		{
			return doesLineSegmentIntersectCircleOrSphere(Eigen::Vector2f(lineSegmentStart[0], lineSegmentStart[1]), Eigen::Vector2f(lineSegmentEnd[0], lineSegmentEnd[1]), boost::get<Eigen::Vector2f>(position), area->comparableSize);
		}
		case STREAMER_AREA_TYPE_CYLINDER:
		{
			Box2D box2D = Box2D(Eigen::Vector2f(boost::get<Eigen::Vector2f>(position)[0] - area->size, boost::get<Eigen::Vector2f>(position)[1] - area->size), Eigen::Vector2f(boost::get<Eigen::Vector2f>(position)[0] + area->size, boost::get<Eigen::Vector2f>(position)[1] + area->size));
			Box3D box3D = Box3D(Eigen::Vector3f(box2D.min_corner()[0], box2D.min_corner()[1], height[0]), Eigen::Vector3f(box2D.max_corner()[0], box2D.max_corner()[1], height[1]));
			return doesLineSegmentIntersectBox(lineSegmentStart, lineSegmentEnd, box3D);
		}
		case STREAMER_AREA_TYPE_SPHERE:
		{
			return doesLineSegmentIntersectCircleOrSphere(lineSegmentStart, lineSegmentEnd, boost::get<Eigen::Vector3f>(position), area->comparableSize);
		}
		case STREAMER_AREA_TYPE_RECTANGLE:
		{
			return doesLineSegmentIntersectBox(Eigen::Vector2f(lineSegmentStart[0], lineSegmentStart[1]), Eigen::Vector2f(lineSegmentEnd[0], lineSegmentEnd[1]), boost::get<Box2D>(position));
		}
		case STREAMER_AREA_TYPE_CUBOID:
		{
			return doesLineSegmentIntersectBox(lineSegmentStart, lineSegmentEnd, boost::get<Box3D>(position));
		}
		case STREAMER_AREA_TYPE_POLYGON:
		{
			Box2D box2D = boost::geometry::return_envelope<Box2D>(boost::get<Polygon2D>(position));
			Box3D box3D = Box3D(Eigen::Vector3f(box2D.min_corner()[0], box2D.min_corner()[1], height[0]), Eigen::Vector3f(box2D.max_corner()[0], box2D.max_corner()[1], height[1]));
			return doesLineSegmentIntersectBox(lineSegmentStart, lineSegmentEnd, box3D);
		}
	}
	return false;
}

bool Utility::isPointInArea(const Eigen::Vector3f &point, const Item::SharedArea &area)
{
	Eigen::Vector2f height = Eigen::Vector2f::Zero();
	boost::variant<Polygon2D, Box2D, Box3D, Eigen::Vector2f, Eigen::Vector3f> position;
	if (area->attach)
	{
		height = area->attach->height;
		position = area->attach->position;
	}
	else
	{
		height = area->height;
		position = area->position;
	}
	switch (area->type)
	{
		case STREAMER_AREA_TYPE_CIRCLE:
		{
			return boost::geometry::comparable_distance(Eigen::Vector2f(point[0], point[1]), boost::get<Eigen::Vector2f>(position)) < area->comparableSize;
		}
		case STREAMER_AREA_TYPE_CYLINDER:
		{
			if ((almostEquals(point[2], height[0]) || (point[2] > height[0])) && (almostEquals(point[2], height[1]) || (point[2] < height[1])))
			{
				return boost::geometry::comparable_distance(Eigen::Vector2f(point[0], point[1]), boost::get<Eigen::Vector2f>(position)) < area->comparableSize;
			}
			return false;
		}
		case STREAMER_AREA_TYPE_SPHERE:
		{
			return boost::geometry::comparable_distance(point, boost::get<Eigen::Vector3f>(position)) < area->comparableSize;
		}
		case STREAMER_AREA_TYPE_RECTANGLE:
		{
			return boost::geometry::covered_by(Eigen::Vector2f(point[0], point[1]), boost::get<Box2D>(position));
		}
		case STREAMER_AREA_TYPE_CUBOID:
		{
			return boost::geometry::covered_by(point, boost::get<Box3D>(position));
		}
		case STREAMER_AREA_TYPE_POLYGON:
		{
			if ((almostEquals(point[2], height[0]) || (point[2] > height[0])) && (almostEquals(point[2], height[1]) || (point[2] < height[1])))
			{
				return boost::geometry::covered_by(Eigen::Vector2f(point[0], point[1]), boost::get<Polygon2D>(position));
			}
			return false;
		}
	}
	return false;
}

void Utility::constructAttachedArea(const Item::SharedArea &area, const boost::variant<float, Eigen::Vector3f, Eigen::Vector4f> &orientation, const Eigen::Vector3f location)
{
	if (area->attach)
	{
		Eigen::Vector3f position = location;
		if (!area->attach->positionOffset.isZero())
		{
			Utility::projectPoint(area->attach->positionOffset, orientation, position);
		}
		switch (area->type)
		{
			case STREAMER_AREA_TYPE_CIRCLE:
			{
				area->attach->position = Eigen::Vector2f(position[0], position[1]);
				break;
			}
			case STREAMER_AREA_TYPE_CYLINDER:
			{
				area->attach->height = Eigen::Vector2f(position[2] + area->height[0], position[2] + area->height[1]);
				area->attach->position = Eigen::Vector2f(position[0], position[1]);
				break;
			}
			case STREAMER_AREA_TYPE_SPHERE:
			{
				area->attach->position = position;
				break;
			}
			case STREAMER_AREA_TYPE_RECTANGLE:
			{
				boost::get<Box2D>(area->attach->position).min_corner() = Eigen::Vector2f(position[0], position[1]) + boost::get<Box2D>(area->position).min_corner();
				boost::get<Box2D>(area->attach->position).max_corner() = Eigen::Vector2f(position[0], position[1]) + boost::get<Box2D>(area->position).max_corner();
				boost::geometry::correct(boost::get<Box2D>(area->attach->position));
				break;
			}
			case STREAMER_AREA_TYPE_CUBOID:
			{
				boost::get<Box3D>(area->attach->position).min_corner() = position + boost::get<Box3D>(area->position).min_corner();
				boost::get<Box3D>(area->attach->position).max_corner() = position + boost::get<Box3D>(area->position).max_corner();
				boost::geometry::correct(boost::get<Box3D>(area->attach->position));
				break;
			}
			case STREAMER_AREA_TYPE_POLYGON:
			{
				area->attach->height = Eigen::Vector2f(position[2] + area->height[0], position[2] + area->height[1]);
				std::vector<Eigen::Vector2f> points;
				for (std::vector<Eigen::Vector2f>::iterator p = boost::get<Polygon2D>(area->position).outer().begin(); p != boost::get<Polygon2D>(area->position).outer().end(); ++p)
				{
					points.push_back(Eigen::Vector2f(position[0], position[1]) + Eigen::Vector2f(p->data()[0], p->data()[1]));
				}
				boost::geometry::assign_points(boost::get<Polygon2D>(area->attach->position), points);
				boost::geometry::correct(boost::get<Polygon2D>(area->attach->position));
				break;
			}
		}
	}
}

void Utility::projectPoint(const Eigen::Vector3f &point, const boost::variant<float, Eigen::Vector3f, Eigen::Vector4f> &orientation, Eigen::Vector3f &position)
{
	switch (orientation.which())
	{
		case 0:
		{
			projectPoint(point, boost::get<float>(orientation), position);
			break;
		}
		case 1:
		{
			projectPoint(point, boost::get<Eigen::Vector3f>(orientation), position);
			break;
		}
		case 2:
		{
			projectPoint(point, boost::get<Eigen::Vector4f>(orientation), position);
			break;
		}
	}
}

void Utility::projectPoint(const Eigen::Vector3f &point, const float &heading, Eigen::Vector3f &position)
{
	float angle = (std::atan2(point[0], point[1]) * (180.0f / (std::atan(1.0f) * 4.0f))) - heading, distance = std::sqrt((point[0] * point[0]) + (point[1] * point[1]));
	position[0] += distance * std::sin(angle * ((std::atan(1.0f) * 4.0f) / 180.0f));
	position[1] += distance * std::cos(angle * ((std::atan(1.0f) * 4.0f) / 180.0f));
	position[2] += point[2];
}

void Utility::projectPoint(const Eigen::Vector3f &point, const Eigen::Vector3f &rotation, Eigen::Vector3f &position)
{
	Eigen::Vector3f rotRad = rotation * ((std::atan(1.0f) * 4.0f) / 180.0f), rotCos(std::cos(rotRad[0]), std::cos(rotRad[1]), std::cos(rotRad[2])), rotSin(std::sin(rotRad[0]), std::sin(rotRad[1]), std::sin(rotRad[2]));
	position[0] += (point[0] * rotCos[1] * rotCos[2]) - (point[0] * rotSin[0] * rotSin[1] * rotSin[2]) - (point[1] * rotCos[0] * rotSin[2]) + (point[2] * rotSin[1] * rotCos[2]) + (point[2] * rotSin[0] * rotCos[1] * rotSin[2]);
	position[1] += (point[0] * rotCos[1] * rotSin[2]) + (point[0] * rotSin[0] * rotSin[1] * rotCos[2]) + (point[1] * rotCos[0] * rotCos[2]) + (point[2] * rotSin[1] * rotSin[2]) - (point[2] * rotSin[0] * rotCos[1] * rotCos[2]);
	position[2] += -(point[0] * rotCos[0] * rotSin[1]) + (point[1] * rotSin[0]) + (point[2] * rotCos[0] * rotCos[1]);
}

void Utility::projectPoint(const Eigen::Vector3f &point, const Eigen::Vector4f &quaternion, Eigen::Vector3f &position)
{
	Eigen::Matrix3f matrix = Eigen::Matrix3f::Zero();
	matrix(0, 0) = 1 - 2 * ((quaternion[1] * quaternion[1]) + (quaternion[2] * quaternion[2]));
	matrix(0, 1) = 2 * ((quaternion[0] * quaternion[1]) - (quaternion[2] * quaternion[3]));
	matrix(0, 2) = 2 * ((quaternion[0] * quaternion[2]) + (quaternion[1] * quaternion[3]));
	matrix(1, 0) = 2 * ((quaternion[0] * quaternion[1]) + (quaternion[2] * quaternion[3]));
	matrix(1, 1) = 1 - 2 * ((quaternion[0] * quaternion[0]) + (quaternion[2] * quaternion[2]));
	matrix(1, 2) = 2 * ((quaternion[1] * quaternion[2]) - (quaternion[0] * quaternion[3]));
	matrix(2, 0) = 2 * ((quaternion[0] * quaternion[2]) - (quaternion[1] * quaternion[3]));
	matrix(2, 1) = 2 * ((quaternion[1] * quaternion[2]) + (quaternion[0] * quaternion[3]));
	matrix(2, 2) = 1 - 2 * ((quaternion[0] * quaternion[0]) + (quaternion[1] * quaternion[1]));
	position[0] += (point[2] * matrix(2, 0)) + (point[1] * matrix(2, 1)) + (point[0] * matrix(2, 2));
	position[1] += -((point[2] * matrix(1, 0)) + (point[1] * matrix(1, 1)) + (point[0] * matrix(1, 2)));
	position[2] += (point[2] * matrix(0, 0)) + (point[1] * matrix(0, 1)) + (point[0] * matrix(0, 2));
}
