// First attempt at a rectangle packer,
// there may be bugs present.
//
// While there are rectangle packing algorithms
// likely way more efficient, this was created
// with the intent of packing on the fly, rather
// than knowing the sizes first hand. Being the
// first iteration, more tests and optimizations
// can be made in the future.

#define RECTPACKER_INCLUDE_TEST

#ifdef RECTPACKER_INCLUDE_TEST
#include <random>
#endif

#include <iostream>
#include <vector>
#include <random>
#include <assert.h>

class RectPacker
{
public:
	struct Transform
	{
		int xOffset, yOffset;
		bool rotated;
	};

	RectPacker(int maxWidth, int maxHeight)
		: m_MaxWidth(maxWidth), m_MaxHeight(maxHeight)
	{
		m_BoundingBoxes.push_back({ maxWidth, maxHeight });
	}

	/// <summary>
	/// Main function which will attempt to pack a 
	/// new rectangle.
	/// </summary>
	/// <param name="width">Width of rectangle</param>
	/// <param name="height">Height of rectangle</param>
	/// <returns>
	/// If successful, returns a transform struct
	/// which contains the offset of the placed rectangle,
	/// as well as if it was rotated. If failed, returns 
	/// a (-1, -1) offset.
	/// </returns>
	Transform Pack(int width, int height)
	{
		std::vector<Fit> fits;

		auto EvaluateBox = [&](BoundingBoxIt box, bool rotate)
		{
			int w = rotate ? height : width;
			int h = rotate ? width : height;

			BoundingBox test = box->CheckFit(w, h);
			if (test.Fits)
				fits.emplace_back(box, GetExposedSurfaceArea({ w, h }, box), false, rotate);
			else
			{
				size_t exposed = TryCombine(box, w, h);
				if (exposed == SIZE_MAX) return;

				fits.emplace_back(box, exposed, true, rotate);
			}
		};

		for (BoundingBoxIt box = m_BoundingBoxes.begin(); box != m_BoundingBoxes.end(); ++box)
		{
			if (box->Filled) continue;
			EvaluateBox(box, false);
			EvaluateBox(box, true);
		}

		if (fits.empty()) return { -1, -1, false };

		return PackMostFit(fits, width, height);
	}

	/// <returns>
	/// Percentage of occupied space
	/// ranging from 0 to 1
	/// </returns>
	float GetOccupancy() const
	{
		float occupiedArea = 0.0f;
		for (const BoundingBox& box : m_BoundingBoxes)
			occupiedArea += box.Filled ? (box.Width * box.Height) : 0.0f;

		return occupiedArea / (m_MaxWidth * m_MaxHeight);
	}

	/// <summary>
	/// Prints the state of the packer to stdout
	/// </summary>
	void Print(bool showFilled)
	{
		char c = 176;
		std::vector<std::vector<char>> data(m_MaxHeight, std::vector<char>(m_MaxWidth, ' '));

		int bx = 0, by = 0;
		for (BoundingBox b : m_BoundingBoxes)
		{
			for (int y = 0; y < b.Height; y++)
				for (int x = 0; x < b.Width; x++)
					data[by + y][bx + x] = b.Filled ? 219 : c;

			if ((bx += b.Width) == m_MaxWidth)
			{
				bx = 0;
				by += b.Height;
			}
			c = c == 177 ? 240 : 177;
		}

		for (const std::vector<char>& row : data)
		{
			std::cout.write(row.data(), row.size());
			std::cout << std::endl;
		}

		std::cout << std::endl;
	}

#ifdef RECTPACKER_INCLUDE_TEST
	/// <summary>
	/// Main testing function to determine the average
	/// efficiency of the packer, using random widths and heights
	/// </summary>
	/// <param name="width">Width of generated packer</param>
	/// <param name="height">Height of generated packer</param>
	/// <param name="iterations">Number of times to test</param>
	static void Test(int width, int height, int iterations)
	{
		std::default_random_engine engine(844);
		std::uniform_int_distribution genWidth(1, width / 2), genHeight(1, height / 2);

		float totalPacks = 0;
		float totalOccupancy = 0;
		for (int i = 0; i < iterations; i++)
		{
			RectPacker test(width, height);

			for (;;)
			{
				int w = genWidth(engine), h = genHeight(engine);
				RectPacker::Transform transform = test.Pack(w, h);
				if (transform.xOffset == -1)
				{
					float occupancy = test.GetOccupancy();
					totalOccupancy += occupancy;

					std::cout << "Occupancy: " << occupancy << std::endl;
					std::cout << std::endl;
					break;
				}

				std::cout << "Packed: " << w << ", " << h << std::endl;
				totalPacks++;
			}

			std::cout << "Result: " << std::endl;
			test.Print(true);
		}

		std::cout << "Average packs: " << totalPacks / iterations << std::endl;
		std::cout << "Average occupancy: " << totalOccupancy / iterations << std::endl;
	}
#endif
private:
	struct BoundingBox
	{
		BoundingBox(int width, int height, bool filled = false)
			: Width(width), Height(height), Filled(filled) {}

		// Check deadspace
		BoundingBox CheckFit(int width, int height)
		{
			int w = Width - width,
				h = Height - height;

			if (w < 0 || h < 0) return { w, h, false };
			return { w, h, true };
		}

		int Width, Height;
		union
		{
			bool Filled;
			bool Fits;
		};
	};

	using BoundingBoxIt = std::vector<BoundingBox>::iterator;
	using Fit = std::tuple<BoundingBoxIt, size_t, bool, bool>;

	Transform GetTransform(BoundingBoxIt it, int width, int height, bool rotated)
	{
		size_t index = std::distance(m_BoundingBoxes.begin(), it);
		int column = index % m_Columns;
		int row = index / m_Columns;
		int x = 0, y = 0;

		for (int i = 0; i < column; i++)
			x += m_BoundingBoxes[i].Width;
		for (int i = 0; i < row; i++)
			y += m_BoundingBoxes[i * m_Columns].Height;

		return { x, y, rotated };
	}

	size_t GetExposedSurfaceArea(
		BoundingBox test,
		BoundingBoxIt box,
		bool left = true,
		bool top = true,
		bool right = true,
		bool bottom = true)
	{
		int64_t index = std::distance(m_BoundingBoxes.begin(), box);
		size_t exposed = 0;

		if (left && (index % m_Columns != 0))
			exposed += (box - 1)->Filled ? 0 : test.Height;

		if (top && (index - m_Columns > 0))
			exposed += (box - m_Columns)->Filled ? 0 : test.Width;

		if (right)
		{
			if (test.Width == box->Width)
			{
				if ((index + 1) % m_Columns != 0 && !(box + 1)->Filled)
					exposed += test.Height;
			}
			else exposed += test.Height;
		}

		if (bottom)
		{
			if (test.Height == box->Height)
			{
				if ((index + m_Columns) < m_BoundingBoxes.size() && !(box + m_Columns)->Filled)
					exposed += test.Width;
			}
			else exposed += test.Width;
		}

		return exposed;

	}

	size_t TryCombine(BoundingBoxIt box, int width, int height)
	{
		auto lastBox = box;
		size_t index = std::distance(m_BoundingBoxes.begin(), box);

		BoundingBox combined = { 0, 0 };
		size_t exposed = 0;
		for (;;)
		{
			int stride = m_Columns, w = 0;
			for (;;)
			{
				BoundingBox diff =
				{
					std::clamp((w + lastBox->Width) - width, 0, lastBox->Width),
					std::clamp((combined.Height + lastBox->Height) - height, 0, lastBox->Height)
				};

				exposed += GetExposedSurfaceArea(
					{
						lastBox->Width - diff.Width,
						lastBox->Height - diff.Height
					},
					lastBox,
					w == 0,
					combined.Height == 0,
					combined.Width + lastBox->Width >= width,
					combined.Height + lastBox->Height >= height
				);

				w += lastBox->Width;
				if (w >= width) break;
				if (++index % m_Columns == 0
					|| (++lastBox)->Filled) return SIZE_MAX;

				stride--;
			}

			combined.Width = w;
			combined.Height += lastBox->Height;

			if (combined.Height >= height) break;
			if ((index += stride) >= m_BoundingBoxes.size()
				|| (lastBox += stride)->Filled) return SIZE_MAX;
		}

		return exposed;
	}

	Transform PackMostFit(std::vector<Fit>& fits, int width, int height)
	{
		// Less exposed surface area is more fit
		std::sort(fits.begin(), fits.end(), [](const Fit& a, const Fit& b) {
			return std::get<size_t>(a) < std::get<size_t>(b);
			});

		BoundingBoxIt mostFit = std::get<BoundingBoxIt>(fits[0]);
		bool rotated = std::get<3>(fits[0]);
		int w = rotated ? height : width,
			h = rotated ? width : height;

		if (std::get<2>(fits[0]))
			return PackCombined(mostFit, w, h, std::get<3>(fits[0]));

		Segment(mostFit, mostFit->CheckFit(w, h));
		mostFit->Filled = true;

		return GetTransform(mostFit, mostFit->Width, mostFit->Height, std::get<3>(fits[0]));
	}

	Transform PackCombined(BoundingBoxIt box, int width, int height, bool rotated)
	{
		size_t index = std::distance(m_BoundingBoxes.begin(), box);
		int originRow = index / m_Columns;

		auto GetCombined = [&](bool fill) -> std::pair<BoundingBox, BoundingBoxIt>
		{
			auto lastBox = box;
			BoundingBox combined = { 0, 0, true };
			for (;;)
			{
				int stride = m_Columns, w = 0;
				for (;;)
				{
					lastBox->Filled = fill;
					w += lastBox->Width;
					if (w >= width) break;
					lastBox++;
					stride--;
				}

				combined.Width = w;
				combined.Height += lastBox->Height;

				if (combined.Height >= height) break;
				lastBox += stride;
			}

			return { combined, lastBox };
		};

		auto data = GetCombined(false);
		int columnsBefore = m_Columns;
		Segment(data.second, { data.first.Width - width, data.first.Height - height });

		// Find origin box
		box = m_BoundingBoxes.begin() + index;
		box += columnsBefore == m_Columns ? 0 : originRow;
		data = GetCombined(true);

		return GetTransform(box, data.first.Width, data.first.Height, rotated);
	}

	void Segment(BoundingBoxIt& box, BoundingBox after) // Bottom right segment
	{
		assert(after.Width >= 0 && after.Height >= 0);
		assert(box->Width >= 1 && box->Height >= 1);
		assert(box->Width > after.Width && box->Height > after.Height);
		if (after.Width == 0 && after.Height == 0) return;

		size_t index = std::distance(m_BoundingBoxes.begin(), box);
		int originRow = (index / m_Columns) + 1;
		if (after.Width) // Create new column
		{
			int row = 0;
			auto emplace = m_BoundingBoxes.begin() + (index % m_Columns++) + 1;
			while (true)
			{
				auto before = emplace - 1;
				before->Width -= after.Width;
				emplace = m_BoundingBoxes.emplace(emplace, after.Width, before->Height, before->Filled);
				if (++row == m_Rows) break;
				if (row < originRow) index++;
				emplace += m_Columns;
			}

			box = m_BoundingBoxes.begin() + index;
		}

		if (after.Height) // Create new row
		{
			m_Rows++;
			auto insert = m_BoundingBoxes.begin() + (originRow * m_Columns);
			insert = m_BoundingBoxes.insert(insert, m_Columns, { 0, 0, false });

			box = m_BoundingBoxes.begin() + index;
			if (insert < box) box += m_Columns;

			for (size_t i = 0; i < m_Columns; i++, insert++)
			{
				auto above = insert - m_Columns;
				above->Height -= after.Height;
				*insert = { above->Width, after.Height, above->Filled };
			}
		}
	}
private:
	int m_MaxWidth, m_MaxHeight;
	int m_Rows = 1, m_Columns = 1;

	std::vector<BoundingBox> m_BoundingBoxes;
};