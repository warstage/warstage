// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GEOMETRY__BOUNDS_H
#define WARSTAGE__GEOMETRY__BOUNDS_H

#include <glm/glm.hpp>


template<class T, glm::precision P>
struct bounds1
{
	using val_t = T;
	using b_1_t = bounds1<T, P>;
	val_t min, max;

	bounds1() : min{}, max{} { }
	explicit bounds1(val_t v) : min{v}, max{v} { }
	bounds1(val_t min_, val_t max_) : min{min_}, max{max_} { }

	template<class T2, glm::precision P2 = P>
	explicit operator bounds1<T2, P2>() const { return {static_cast<T2>(min), static_cast<T2>(max)}; }

	val_t size() const { return max - min; }
	val_t radius() const { return (max - min) / 2; }

	bool empty() const { return min >= max; }
	bool contains(val_t v) const { return min <= v && v <= max; }

	bool intersects(const b_1_t& b) const
	{
		bool disjoint_left = max < b.min;
		bool disjoint_right = b.max < min;
		bool disjoint = disjoint_left || disjoint_right;
		return !disjoint;
	}

	val_t distance(val_t v) const { return v < min ? v - min : v > max ? v - max : val_t{}; };

	b_1_t add(val_t v) const { return {min + v, max + v}; }
	b_1_t sub(val_t v) const { return {min - v, max - v}; }
	b_1_t mul(val_t v) const { return {min * v, max * v}; }
	b_1_t div(val_t v) const { return {min / v, max / v}; }

	b_1_t add_radius(val_t d) const { return {min - d, max + d}; }
	b_1_t add_radius(val_t d, val_t a) const { val_t m{mix(a)}; val_t s{size()}; return {m - s * a - d, m + s * (1 - a) + d}; };

	b_1_t set_size(val_t s) const { val_t m{mid() - s / 2}; return {m, m + s}; };
	b_1_t set_size(val_t s, val_t a) const { val_t m{mix(a) - s * a}; return {m, m + s}; };

	val_t mid() const { return (min + max) / 2; }
	val_t mix(val_t v) const { return min + v * (max - min); }
	val_t unmix(val_t v) const { return (v - min) / (max - min); }

	val_t clamp(val_t v) const {
		return min > max ? (min + max) / 2 :
			v < min ? min :
			v > max ? max :
			v;
	}

    b_1_t clamp(b_1_t v) const { return {clamp(v.min), clamp(v.max)}; }

	b_1_t& operator+=(val_t v)
	{
		min += v;
		max += v;
		return *this;
	}

	b_1_t& operator-=(val_t v)
	{
		min -= v;
		max -= v;
		return *this;
	}
};


template<class T, glm::precision P>
struct bounds2
{
	using val_t = T;
	using vec_t = glm::tvec2<T, P>;
	using b_1_t = bounds1<T, P>;
	using b_2_t = bounds2<T, P>;

	vec_t min, max;

	bounds2() : min{}, max{} { }
	explicit bounds2(vec_t v) : min{v}, max{v} { }

	bounds2(val_t min_x, val_t min_y, val_t max_x, val_t max_y) : min{min_x, min_y}, max{max_x, max_y} { }
	bounds2(val_t min_x, val_t min_y, vec_t max_) : min{min_x, min_y}, max{max_} { }
	bounds2(val_t min_x, val_t max_x, b_1_t y) : min{min_x, y.min}, max{max_x, y.max} { }
	bounds2(b_1_t x, val_t min_y, val_t max_y) : min{x.min, min_y}, max{x.max, max_y} { }
	bounds2(val_t x, b_1_t y) : min{x, y.min}, max{x, y.max} { }
	bounds2(b_1_t x, val_t y) : min{x.min, y}, max{x.max, y} { }
	bounds2(vec_t min_, vec_t max_) : min{min_}, max{max_} { }
	bounds2(b_1_t x, b_1_t y) : min{x.min, y.min}, max{x.max, y.max} { }
	bounds2(val_t x, val_t y) : min{x, y}, max{x, y} { }

	template<class T2, glm::precision P2 = P>
	explicit operator bounds2<T2, P2>() const { return {static_cast<glm::tvec2<T2, P2>>(min), static_cast<glm::tvec2<T2, P2>>(max)}; }

	b_1_t x() const { return {min.x, max.x}; }
	b_1_t y() const { return {min.y, max.y}; }

	vec_t mid() const { return {(min.x + max.x) / 2, (min.y + max.y) / 2}; }
	vec_t mix(vec_t a) const { return {x().mix(a.x), y().mix(a.y)}; }
	vec_t unmix(vec_t p) const { return {x().unmix(p.x), y().unmix(p.y)}; }

	template <int X, int Y> vec_t fix() const { return {(1 - X) * min.x + X * max.x, (1 - Y) * min.y + Y * max.y}; }

	vec_t size() const { return max - min; }
	vec_t radius() const { return {(max.x - min.x) / 2, (max.y - min.y) / 2}; }

	bool empty() const { return min.x >= max.x || min.y >= max.y; }
	bool contains(vec_t p) const { return min.x <= p.x && p.x <= max.x && min.y <= p.y && p.y <= max.y; }
	bool intersects(const b_2_t& b) const { return x().intersects(b.x()) && y().intersects(b.y()); }

	vec_t distance(vec_t p) const { return {x().distance(p.x), y().distance(p.y)}; }

	b_2_t set_min(vec_t v) const { return {v, max}; }
	b_2_t set_max(vec_t v) const { return {min, v}; }
	b_2_t set_mid(vec_t v) const { return sub(mid()).add(v); }

	b_2_t set_min_x(val_t v) const { return {v, min.y, max.x, max.y}; }
	b_2_t set_min_y(val_t v) const { return {min.x, v, max.x, max.y}; }
	b_2_t set_max_x(val_t v) const { return {min.x, min.y, v, max.y}; }
	b_2_t set_max_y(val_t v) const { return {min.x, min.y, max.x, v}; }

	b_2_t set_x(val_t v) const { return {v, y()}; }
	b_2_t set_x(val_t x_min, val_t x_max) const { return {x_min, x_max, y()}; }
	b_2_t set_x(b_1_t v) const { return {v, y()}; }

	b_2_t set_y(val_t v) const { return {x(), v}; }
	b_2_t set_y(val_t y_min, val_t y_max) const { return {x(), y_min, y_max}; }
	b_2_t set_y(b_1_t v) const { return {x(), v}; }

	b_2_t add_x(val_t v) const { return {min.x + v, min.y, max.x + v, max.y}; }
	b_2_t add_y(val_t v) const { return {min.x, min.y + v, max.x, max.y + v}; }
	b_2_t sub_x(val_t v) const { return {min.x - v, min.y, max.x - v, max.y}; }
	b_2_t sub_y(val_t v) const { return {min.x, min.y - v, max.x, max.y - v}; }

	b_2_t add_min_x(val_t v) const { return {min.x + v, min.y, max.x, max.y}; }
	b_2_t add_min_y(val_t v) const { return {min.x, min.y + v, max.x, max.y}; }
	b_2_t add_max_x(val_t v) const { return {min.x, min.y, max.x + v, max.y}; }
	b_2_t add_max_y(val_t v) const { return {min.x, min.y, max.x, max.y + v}; }

	b_2_t sub_min_x(val_t v) const { return {min.x - v, min.y, max.x, max.y}; }
	b_2_t sub_min_y(val_t v) const { return {min.x, min.y - v, max.x, max.y}; }
	b_2_t sub_max_x(val_t v) const { return {min.x, min.y, max.x - v, max.y}; }
	b_2_t sub_max_y(val_t v) const { return {min.x, min.y, max.x, max.y - v}; }

	b_2_t add(val_t v) const { return {min + v, max + v}; }
	b_2_t sub(val_t v) const { return {min - v, max - v}; }
	b_2_t mul(val_t v) const { return {min * v, max * v}; }
	b_2_t div(val_t v) const { return {min / v, max / v}; }

	b_2_t add(vec_t v) const { return {min + v, max + v}; }
	b_2_t sub(vec_t v) const { return {min - v, max - v}; }
	b_2_t mul(vec_t v) const { return {min * v, max * v}; }
	b_2_t div(vec_t v) const { return {min / v, max / v}; }

	b_2_t add_radius(val_t d) const { return {min - d, max + d}; }
	b_2_t add_radius(vec_t d) const { return {min - d, max + d}; }
	b_2_t add_radius(val_t d, vec_t a) const { return {x().add_radius(d, a.x), y().add_radius(d, a.y)}; }
	b_2_t add_radius(vec_t d, vec_t a) const { return {x().add_radius(d.x, a.x), y().add_radius(d.y, a.y)}; }

	b_2_t add_radius_x(val_t d) const { return {x().add_radius(d), y()}; }
	b_2_t add_radius_y(val_t d) const { return {x(), y().add_radius(d)}; }
	b_2_t add_radius_x(val_t d, val_t a) const { return {x().add_radius(d, a), y()}; }
	b_2_t add_radius_y(val_t d, val_t a) const { return {x(), y().add_radius(d, a)}; }

	b_2_t set_size(val_t s) const { return {x().set_size(s), y().set_size(s)}; }
	b_2_t set_size(vec_t s) const { return {x().set_size(s.x), y().set_size(s.y)}; }
	b_2_t set_size(val_t s, vec_t a) const { return {x().set_size(s, a.x), y().set_size(s, a.y)}; }
	b_2_t set_size(vec_t s, vec_t a) const { return {x().set_size(s.x, a.x), y().set_size(s.y, a.y)}; }

	b_2_t set_size_x(val_t s) const { return {x().set_size(s), y()}; }
	b_2_t set_size_y(val_t s) const { return {x(), y().set_size(s)}; }
	b_2_t set_size_x(val_t s, val_t a) const { return {x().set_size(s, a), y()}; }
	b_2_t set_size_y(val_t s, val_t a) const { return {x(), y().set_size(s, a)}; }

	vec_t clamp(vec_t p) const { return {x().clamp(p.x), y().clamp(p.y)}; }
    b_2_t clamp(b_2_t v) const { return {clamp(v.min), clamp(v.max)}; }

	bounds2& operator+=(vec_t v)
	{
		min += v;
		max += v;
		return *this;
	}
	bounds2& operator-=(vec_t v)
	{
		min -= v;
		max -= v;
		return *this;
	}
	bounds2& operator*=(vec_t v)
	{
		min *= v;
		max *= v;
		return *this;
	}
	bounds2& operator/=(vec_t v)
	{
		min /= v;
		max /= v;
		return *this;
	}

	bounds2& operator*=(val_t k)
	{
		min *= k;
		max *= k;
		return *this;
	}
	bounds2& operator/=(val_t k)
	{
		min /= k;
		max /= k;
		return *this;
	}
};


template<class T, glm::precision P>
struct bounds3
{
    using val_t = T;
    using vec_t = glm::tvec3<T, P>;
	using b_1_t = bounds1<T, P>;
	using b_2_t = bounds2<T, P>;
	using b_3_t = bounds3<T, P>;

	vec_t min, max;

	bounds3() : min{}, max{} { }
	explicit bounds3(vec_t v) : min{v}, max{v} { }

	bounds3(vec_t min_, vec_t max_) : min{min_}, max{max_} { }
	bounds3(b_2_t b, b_1_t z) : min{b.min, z.min}, max{b.max, z.max} { }
    bounds3(b_2_t b, val_t z_min, val_t z_max) : min{b.min, z_min}, max{b.max, z_max} { }
	bounds3(b_1_t x, b_1_t y, b_1_t z) : min{x.min, y.min, z.min}, max{x.max, y.max, z.max} { }

	template<class T2, glm::precision P2 = P>
	explicit operator bounds3<T2, P2>() const { return {static_cast<glm::tvec3<T2, P2>>(min), static_cast<glm::tvec3<T2, P2>>(max)}; }

	bool empty() const { return min.x >= max.x || min.y >= max.y || min.z >= max.z; }
	bool contains(vec_t p) const { return min.x <= p.x && p.x <= max.x && min.y <= p.y && p.y <= max.y && min.z <= p.z && p.z <= max.z; }

	b_2_t xy() const { return {min.x, min.y, max.x, max.y}; }

    vec_t size() const { return max - min; }
};


template<class T, glm::precision P>
inline bool operator==(bounds1<T, P> a, bounds1<T, P> b) { return a.min == b.min && a.max == b.max; }


template<class T, glm::precision P>
inline bool operator==(bounds2<T, P> a, bounds2<T, P> b) { return a.min == b.min && a.max == b.max; }


template<class T, glm::precision P>
inline bool operator==(bounds3<T, P> a, bounds3<T, P> b) { return a.min == b.min && a.max == b.max; }


template<class T, glm::precision P>
inline bool operator!=(bounds1<T, P> a, bounds1<T, P> b) { return a.min != b.min || a.max != b.max; }


template<class T, glm::precision P>
inline bool operator!=(bounds2<T, P> a, bounds2<T, P> b) { return a.min != b.min || a.max != b.max; }


template<class T, glm::precision P>
inline bool operator!=(bounds3<T, P> a, bounds3<T, P> b) { return a.min != b.min || a.max != b.max; }


template<class T, glm::precision P>
inline bounds1<T, P> operator+(bounds1<T, P> b, T k) { return {b.min + k, b.max + k}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator+(bounds2<T, P> b, glm::tvec2<T, P> v) { return {b.min + v, b.max + v}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator+(bounds3<T, P> b, glm::tvec3<T, P> v) { return {b.min + v, b.max + v}; }


template<class T, glm::precision P>
inline bounds1<T, P> operator+(bounds1<T, P> b1, bounds1<T, P> b2) { return {b1.min + b2.min, b1.max + b2.max}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator+(bounds2<T, P> b1, bounds2<T, P> b2) { return {b1.min + b2.min, b1.max + b2.max}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator+(bounds3<T, P> b1, bounds3<T, P> b2) { return {b1.min + b2.min, b1.max + b2.max}; }


template<class T, glm::precision P>
inline bounds1<T, P> operator-(bounds1<T, P> b, T k) { return {b.min - k, b.max - k}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator-(bounds2<T, P> b, glm::tvec2<T, P> v) { return {b.min - v, b.max - v}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator-(bounds3<T, P> b, glm::tvec3<T, P> v) { return {b.min - v, b.max - v}; }


template<class T, glm::precision P>
inline bounds1<T, P> operator-(bounds1<T, P> b1, bounds1<T, P> b2) { return {b1.min - b2.min, b1.max - b2.max}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator-(bounds2<T, P> b1, bounds2<T, P> b2) { return {b1.min - b2.min, b1.max - b2.max}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator-(bounds3<T, P> b1, bounds3<T, P> b2) { return {b1.min - b2.min, b1.max - b2.max}; }


template<class T, glm::precision P>
inline bounds1<T, P> operator*(bounds1<T, P> b, T k) { return {b.min * k, b.max * k}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator*(bounds2<T, P> b, T k) { return {b.min * k, b.max * k}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator*(bounds3<T, P> b, T k) { return {b.min * k, b.max * k}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator*(bounds2<T, P> b, glm::tvec2<T, P> v) { return {b.min * v, b.max * v}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator*(bounds3<T, P> b, glm::tvec3<T, P> v) { return {b.min * v, b.max * v}; }


template<class T, glm::precision P>
inline bounds1<T, P> operator*(T k, bounds1<T, P> b) { return {k * b.min, k * b.max}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator*(T k, bounds2<T, P> b) { return {k * b.min, k * b.max}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator*(T k, bounds3<T, P> b) { return {k * b.min, k * b.max}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator*(glm::tvec2<T, P> v, bounds2<T, P> b) { return {v * b.min, v * b.max}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator*(glm::tvec3<T, P> v, bounds3<T, P> b) { return {v * b.min, v * b.max}; }


template<class T, glm::precision P>
inline bounds1<T, P> operator/(bounds1<T, P> b, T k) { return {b.min / k, b.max / k}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator/(bounds2<T, P> b, T k) { return {b.min / k, b.max / k}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator/(bounds3<T, P> b, T k) { return {b.min / k, b.max / k}; }


template<class T, glm::precision P>
inline bounds2<T, P> operator/(bounds2<T, P> b, glm::tvec2<T, P> v) { return {b.min / v, b.max / v}; }


template<class T, glm::precision P>
inline bounds3<T, P> operator/(bounds3<T, P> b, glm::tvec3<T, P> v) { return {b.min / v, b.max / v}; }


using bounds1d = bounds1<double, glm::highp>;
using bounds2d = bounds2<double, glm::highp>;
using bounds3d = bounds3<double, glm::highp>;

using bounds1f = bounds1<float, glm::highp>;
using bounds2f = bounds2<float, glm::highp>;
using bounds3f = bounds3<float, glm::highp>;

using bounds1i = bounds1<int, glm::highp>;
using bounds2i = bounds2<int, glm::highp>;
using bounds3i = bounds3<int, glm::highp>;


#endif
