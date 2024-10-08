#include "global_execution_context.h"

#include "utils.h"

template<typename TreeKernel>
global_execution_context<TreeKernel>::global_execution_context()
#ifdef GEOBIM_DEBUG
	: obj_("debug-tree")
#endif
{
	// style 0 is for elements without style annotations
	infos.push_back(nullptr);
}

template<typename TreeKernel>
void global_execution_context<TreeKernel>::finalize() {
	tree.build();
	tree.accelerate_distance_queries();
}

// @todo this should be merged with the other implementation below.
template<typename TreeKernel>
typename global_execution_context<TreeKernel>::segmentation_return_type_2 global_execution_context<TreeKernel>::segment(const non_manifold_polyhedron<CGAL::Simple_cartesian<double>>& input) {
	segmentation_return_type_2 result(infos.size());
	auto it = infos.begin();
	for (size_t i = 0; i < infos.size(); ++i, ++it) {
		result[i].first = i ? *it : nullptr;
	}

	CGAL::Cartesian_converter<CGAL::Simple_cartesian<double>, TreeKernel> converter;

	for (auto it = input.indices.begin(); it != input.indices.end(); ++it) {

		auto f_c = CGAL::centroid(
			input.points[(*it)[0]],
			input.points[(*it)[1]],
			input.points[(*it)[2]]
		);

		auto f_c_ic = converter(f_c);

		auto pair = tree.closest_point_and_primitive(f_c_ic);
		typename TreeShapeType::Face_handle F = pair.second.first;
		auto jt = facet_to_info.find(F);
		if (jt != facet_to_info.end()) {
			int sid = std::distance(infos.cbegin(), std::find(infos.cbegin(), infos.cend(), jt->second));
			result[sid].second.push_back(it);
		}
	}

	return result;
}

template<typename TreeKernel>
typename global_execution_context<TreeKernel>::segmentation_return_type global_execution_context<TreeKernel>::segment(const cgal_shape_t & input) {
	segmentation_return_type result(infos.size());
	auto it = infos.begin();
	for (size_t i = 0; i < infos.size(); ++i, ++it) {
		result[i].first = i ? *it : nullptr;
	}

	CGAL::Cartesian_converter<Kernel_, TreeKernel> converter;

	for (auto &f : faces(input)) {

		auto f_c = CGAL::centroid(
			f->facet_begin()->vertex()->point(),
			f->facet_begin()->next()->vertex()->point(),
			f->facet_begin()->next()->next()->vertex()->point()
		);

		auto f_c_ic = converter(f_c);

#if 0
		// Find co-planar facets from original IFC geometries within bounding box of
		// nef-constructed facet. Filter based on facet normal and distance along normal.

		std::list<Primitive_id> primitives, coplanar_primitives;
		Bounding_box f_bbox = CGAL::Polygon_mesh_processing::face_bbox(f, input);
		tree.all_intersected_primitives(f_bbox, std::back_inserter(primitives));

		auto f_norm = CGAL::Polygon_mesh_processing::compute_face_normal(f, input);
		auto f_norm_ic = converter(f_norm);

		for (auto& p : primitives) {
			auto p_norm_ic = CGAL::Polygon_mesh_processing::compute_face_normal(p.first, *p.second);
			auto dot = std::abs(CGAL::to_double(f_norm_ic * p_norm_ic));
			if (dot > 0.999) {
				auto p_p0_ic = p.first->facet_begin()->vertex()->point();
				auto d = std::abs(CGAL::to_double((p_p0_ic - f_c_ic) * p_norm_ic));
				if (d < 0.001) {
					coplanar_primitives.push_back(p);
				}
			}
		}

		std::cout << "bbox " << primitives.size() << " coplanar " << coplanar_primitives.size() << std::endl;
#endif

		auto pair = tree.closest_point_and_primitive(f_c_ic);
		typename TreeShapeType::Face_handle F = pair.second.first;
		auto it = facet_to_info.find(F);
		if (it != facet_to_info.end()) {
			int sid = std::distance(infos.cbegin(), std::find(infos.cbegin(), infos.cend(), it->second));
			result[sid].second.push_back(f);
		}
	}

	return result;
}

template<typename TreeKernel>
void global_execution_context<TreeKernel>::operator()(shape_callback_item* item) {

	item_info* info = new item_info{
		(int) item->src->id(),
		// @todo
		0, // parent_id
		item->src->declaration().name(),
		item->src->get_value<std::string>("GlobalId"),
		item->src->get_value<std::string>("Name", ""),
		item->style
	};
	infos.push_back(info);

	// typename TreeKernel::Aff_transformation_3 transformation;
	// util::copy::transformation(transformation, item->transformation);

	std::transform(
		item->polyhedron.points_begin(), item->polyhedron.points_end(),
		item->polyhedron.points_begin(), item->transformation);

	TreeShapeType tree_polyhedron;
	util::copy::polyhedron(tree_polyhedron, item->polyhedron);

	triangulated_shape_memory.push_back(tree_polyhedron);
	CGAL::Polygon_mesh_processing::triangulate_faces(triangulated_shape_memory.back());
	tree.insert(faces(triangulated_shape_memory.back()).first, faces(triangulated_shape_memory.back()).second, triangulated_shape_memory.back());

#ifdef GEOBIM_DEBUG
	obj_(nullptr, triangulated_shape_memory.back().facets_begin(), triangulated_shape_memory.back().facets_end());
#endif

	for (auto& f : faces(triangulated_shape_memory.back())) {
		typename TreeShapeType::Facet_handle F = f;
		facet_to_info.insert({ F, info });
	}
}

template struct global_execution_context<CGAL::Epick>;
template struct global_execution_context<CGAL::Epeck>;
template struct global_execution_context<CGAL::Simple_cartesian<double>>;
