#include "solver.h"
#include "cube.h"
#include <queue>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <future>
#include <atomic>
#include <mutex>
#include <map>
#ifdef _WIN32
	#include <Debug.h>
#else
	#include "/run/media/trezowave/Data/Workspace/CPP_Projects/Debug.h"
#endif
#include <clocale>

using std::map;

constexpr uint32_t FACT[13]{ 1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800, 39916800, 479001600 };

constexpr std::array<int, 6> EDGE_SET_1{ 0, 1, 2, 3, 8, 9 };    // UR, UF, UL, UB, FR, FL
constexpr std::array<int, 6> EDGE_SET_2{ 4, 5, 6, 7, 10, 11 };  // DR, DF, DL, DB, BL, BR

uint8_t Solver::cpMove[18][8]{}, Solver::coMove[18][8]{};
uint8_t Solver::epMove[18][12]{}, Solver::eoMove[18][12]{};

namespace {
	const uint8_t* ptrCorner = nullptr;
	const uint8_t* ptrEdge1 = nullptr;
	const uint8_t* ptrEdge2 = nullptr;
}

Solver::Solver() {
	static bool initialized{ false };
	if (!initialized) {
		Solver::initMoveTables();

		if (!loadPruneTables())
		{
			LOG("PDB files not found. Generating (this will take a while)...\n");
			generatePruneTables();
			savePruneTables();
		}
		initialized = true;
	}

	ptrCorner = Solver::getDistCorner().data();
	ptrEdge1 = Solver::getDistEdge1().data();
	ptrEdge2 = Solver::getDistEdge2().data();
}

void Solver::initMoveTables() {
	alignas(Cube) char rawCube[sizeof(Cube)]{};
	Cube* cube{ reinterpret_cast<Cube*>(rawCube) };

	for (int m{ 0 }; m < 18; ++m)
	{
		for (int x{ -1 }; x < 2; ++x)
		{
			for (int y{ -1 }; y < 2; ++y)
			{
				for (int z{ -1 }; z < 2; ++z)
				{
					std::array<int, 3> asis{ y, x, z };
					for (int i{ 0 }; i < 6; ++i)
					{
						cube->cubies[x + 1][y + 1][z + 1].colors[i] =
							(asis[i / 2] == SIGNS[i % 2]) ? FACE_COLORS[i] : BLACK;
					}
				}
			}
		}

		int face{ m / 3 };
		int pow{ (m % 3) + 1 };
		int axis{ 0 }, layer{ 0 };

		if (face == 0) { axis = 1; layer = 2; }
		else if (face == 1) { axis = 0; layer = 2; }
		else if (face == 2) { axis = 2; layer = 2; }
		else if (face == 3) { axis = 1; layer = 0; }
		else if (face == 4) { axis = 0; layer = 0; }
		else if (face == 5) { axis = 2; layer = 0; }

		int turns{ pow };
		if (face == 3 || face == 4 || face == 5) turns = 4 - pow;

		auto temp{ cube->cubies };
		for (int x{ 0 }; x < 3; ++x)
		{
			for (int y{ 0 }; y < 3; ++y)
			{
				for (int z{ 0 }; z < 3; ++z)
				{
					if (axis == 0 && x != layer) continue;
					if (axis == 1 && y != layer) continue;
					if (axis == 2 && z != layer) continue;

					int sx{ x }, sy{ y }, sz{ z };
					for (int t{ 0 }; t < turns; ++t)
					{
						if (axis == 0) { int ts{ sy }; sy = 2 - sz; sz = ts; }
						else if (axis == 1) { int ts{ sx }; sx = sz; sz = 2 - ts; }
						else if (axis == 2) { int ts{ sx }; sx = 2 - sy; sy = ts; }
					}
					cube->cubies[x][y][z] = temp[sx][sy][sz];

					for (int t{ 0 }; t < turns; ++t) {
						auto c{ cube->cubies[x][y][z].colors };

						if (axis == 0) {
							cube->cubies[x][y][z].colors[0] = c[4];
							cube->cubies[x][y][z].colors[5] = c[0];
							cube->cubies[x][y][z].colors[1] = c[5];
							cube->cubies[x][y][z].colors[4] = c[1];
						}
						else if (axis == 1) {
							cube->cubies[x][y][z].colors[4] = c[2];
							cube->cubies[x][y][z].colors[3] = c[4];
							cube->cubies[x][y][z].colors[5] = c[3];
							cube->cubies[x][y][z].colors[2] = c[5];
						}
						else if (axis == 2) {
							cube->cubies[x][y][z].colors[2] = c[0];
							cube->cubies[x][y][z].colors[1] = c[2];
							cube->cubies[x][y][z].colors[3] = c[1];
							cube->cubies[x][y][z].colors[0] = c[3];
						}
					}
				}
			}
		}

		SearchState s{};
		s.fromCube(*cube);

		for (int i{ 0 }; i < 8; ++i)
		{
			Solver::cpMove[m][i] = s.cp[i];
			Solver::coMove[m][i] = s.co[i];
		}
		for (int i{ 0 }; i < 12; ++i)
		{
			Solver::epMove[m][i] = s.ep[i];
			Solver::eoMove[m][i] = s.eo[i];
		}
	}
}

static void bfsDistLargeMT(std::vector<uint8_t>& table, size_t size,
	std::function<uint32_t(const SearchState&)> idFunc) {

	table.assign(size, 0xFF);
	std::vector<SearchState> currentFrontier{};

	SearchState goal{};
	for (int i{ 0 }; i < 8; ++i) { goal.cp[i] = i; goal.co[i] = 0; }
	for (int i{ 0 }; i < 12; ++i) { goal.ep[i] = i; goal.eo[i] = 0; }

	table[idFunc(goal)] = 0;
	currentFrontier.push_back(goal);

	int depth{ 0 };
	size_t totalGenerated{ 1 };

	int numThreads{ static_cast<int>(std::thread::hardware_concurrency()) };
	if (numThreads == 0) numThreads = 8;

	while (!currentFrontier.empty()) {
		LOG("Generating PDB... Depth: {}, Generated: {} / {}\n", depth, totalGenerated, size);

		std::vector<std::vector<SearchState>> threadLocalFrontiers(numThreads);
		std::vector<std::future<void>> futures{};

		size_t chunkSize{ currentFrontier.size() / numThreads + 1 };

		for (int t{ 0 }; t < numThreads; ++t) {
			futures.push_back(std::async(std::launch::async, [&, t]() {
				size_t start{ t * chunkSize };
				size_t end{ std::min(start + chunkSize, currentFrontier.size()) };

				for (size_t i{ start }; i < end; ++i) {
					const SearchState& cur{ currentFrontier[i] };

					for (Move m : Solver::getAllMoves()) {
						SearchState next{ cur };
						next.apply(m);
						uint32_t nextId{ idFunc(next) };

						auto* cell{ reinterpret_cast<std::atomic<uint8_t>*>(&table[nextId]) };
						uint8_t expected{ 0xFF };

						if (cell->compare_exchange_strong(expected, static_cast<uint8_t>(depth + 1))) {
							threadLocalFrontiers[t].push_back(next);
						}
					}
				}
				}));
		}

		for (auto& f : futures) f.wait();

		currentFrontier.clear();
		for (int t{ 0 }; t < numThreads; ++t) {
			totalGenerated += threadLocalFrontiers[t].size();
			currentFrontier.insert(currentFrontier.end(),
				std::make_move_iterator(threadLocalFrontiers[t].begin()),
				std::make_move_iterator(threadLocalFrontiers[t].end()));
		}
		depth++;
	}
	LOG("PDB Generation Complete!\n");
}

void Solver::generatePruneTables() {
	bfsDistLargeMT(getDistCorner(), 88179840, idCorner);
	bfsDistLargeMT(getDistEdge1(), 42577920, idEdge1);
	bfsDistLargeMT(getDistEdge2(), 42577920, idEdge2);
}

bool Solver::loadPruneTables() {
	auto loadVec = [](const char* fname, std::vector<uint8_t>& vec, size_t expected) -> bool {
		std::ifstream in{ fname, std::ios::binary };
		if (!in) return false;
		vec.resize(expected);
		in.read(reinterpret_cast<char*>(vec.data()), expected);
		return in.good();
		};
	return loadVec("pdb_corner.bin", getDistCorner(), 88179840) &&
		loadVec("pdb_edge1.bin", getDistEdge1(), 42577920) &&
		loadVec("pdb_edge2.bin", getDistEdge2(), 42577920);
}

void Solver::savePruneTables() {
	auto saveVec = [](const char* fname, const std::vector<uint8_t>& vec) {
		std::ofstream out{ fname, std::ios::binary };
		out.write(reinterpret_cast<const char*>(vec.data()), vec.size());
		};
	saveVec("pdb_corner.bin", getDistCorner());
	saveVec("pdb_edge1.bin", getDistEdge1());
	saveVec("pdb_edge2.bin", getDistEdge2());
}

void SearchState::apply(Move m, bool rorateOnly)
{
	if (!isOptimal) path.push_back(m);
	if (rorateOnly) return;
	int idx{ static_cast<int>(m) };
	if (idx >= 18) return;

	uint8_t oldCp[8], oldCo[8], oldEp[12], oldEo[12];
	std::memcpy(oldCp, cp, 8);
	std::memcpy(oldCo, co, 8);
	std::memcpy(oldEp, ep, 12);
	std::memcpy(oldEo, eo, 12);

	for (int i{ 0 }; i < 8; ++i)
	{
		uint8_t src{ Solver::cpMove[idx][i] };
		cp[i] = oldCp[src];
		uint8_t sum = oldCo[src] + Solver::coMove[idx][i];
		co[i] = (sum >= 3) ? (sum - 3) : sum;
	}

	for (int i{ 0 }; i < 12; ++i)
	{
		uint8_t src{ Solver::epMove[idx][i] };
		ep[i] = oldEp[src];
		eo[i] = oldEo[src] ^ Solver::eoMove[idx][i];
	}
}

void SearchState::apply(const vector<Move>& moves)
{
	for (const auto& m : moves) apply(m);
}

void SearchState::fromCube(const Cube& cube)
{
	const Color colors[6]{ RED, WHITE, BLUE, ORANGE, YELLOW, GREEN };
	map<Color, int> colorToFace{};
	for (int i{ 0 }; i < 6; ++i) colorToFace[colors[i]] = i;

	Color colorU{ cube.cubies[1][2][1].colors[0] };
	Color colorR{ cube.cubies[2][1][1].colors[2] };
	Color colorF{ cube.cubies[1][1][2].colors[4] };
	Color colorD{ cube.cubies[1][0][1].colors[1] };
	Color colorL{ cube.cubies[0][1][1].colors[3] };
	Color colorB{ cube.cubies[1][1][0].colors[5] };

	map<Color, uint8_t> faceMask{};
	faceMask[colorU] = 1; faceMask[colorR] = 2; faceMask[colorF] = 4;
	faceMask[colorD] = 8; faceMask[colorL] = 16; faceMask[colorB] = 32;

	int idx{ 0 };
	uint8_t Fs[54]{};

	const int colorMap[6]{ 0, 2, 4, 1, 3, 5 };
	for (int face{ 0 }; face < 6; ++face)
	{
		for (int row{ 0 }; row < 3; ++row)
		{
			for (int col{ 0 }; col < 3; ++col)
			{
				int x{ 0 }, y{ 0 }, z{ 0 };
				if		(face == 0) { x =	  col; y =		 2; z =		row; }
				else if (face == 1) { x =		2; y = 2 - row; z = 2 - col; }
				else if (face == 2) { x =	  col; y = 2 - row; z =		  2; }
				else if (face == 3) { x =	  col; y =		 0; z = 2 - row; }
				else if (face == 4) { x =		0; y = 2 - row; z =		col; }
				else if (face == 5) { x = 2 - col; y = 2 - row; z =		  0; }

				int colorIdx{ colorMap[face] };
				Color c{ cube.cubies[x][y][z].colors[colorIdx] };
				Fs[idx++] = faceMask[c];
			}
		}
	}

	const uint8_t U{ 1 }, R{ 2 }, F{ 4 }, D{ 8 }, L{ 16 }, B{ 32 };

	static constexpr auto CIdxLUT
	{
		[]()
		{
		std::array<int, 64> arr{};
		arr[U | R | F] = 0; arr[U | F | L] = 1; arr[U | L | B] = 2; arr[U | B | R] = 3;
		arr[D | F | R] = 4; arr[D | L | F] = 5; arr[D | B | L] = 6; arr[D | R | B] = 7;
		return arr;
		}()
	};

	static constexpr auto EIdxLUT
	{
		[]()
		{
		std::array<int, 64> arr{};
		arr[U | R] = 0; arr[U | F] = 1; arr[U | L] = 2; arr[U | B] = 3;
		arr[D | R] = 4; arr[D | F] = 5; arr[D | L] = 6; arr[D | B] = 7;
		arr[F | R] = 8; arr[F | L] = 9; arr[B | L] = 10; arr[B | R] = 11;
		return arr;
		}()
	};

	constexpr int CFIdx[8][3]
	{
		{8,  9, 20}, {6, 18, 38}, {0, 36, 47}, {2, 45, 11},
		{29,26, 15}, {27,44, 24}, {33,53, 42}, {35,17, 51}
	};
	constexpr int EFIdx[12][2]
	{
		{5,10}, {7,19}, {3,37}, {1,46},
		{32,16}, {28,25}, {30,43}, {34,52},
		{23,12}, {21,41}, {50,39}, {48,14}
	};

	for (int i{ 0 }; i < 8; ++i)
	{
		uint8_t f0{ Fs[CFIdx[i][0]] };
		uint8_t f1{ Fs[CFIdx[i][1]] };
		uint8_t f2{ Fs[CFIdx[i][2]] };
		cp[i] = CIdxLUT[f0 | f1 | f2];

		if (f1 & (U | D)) co[i] = 1;
		else if (f2 & (U | D)) co[i] = 2;
		else co[i] = 0;
	}

	for (int i{ 0 }; i < 12; ++i)
	{
		uint8_t e0{ Fs[EFIdx[i][0]] };
		uint8_t e1{ Fs[EFIdx[i][1]] };
		ep[i] = EIdxLUT[e0 | e1];
		bool good{ (e0 & (U | D)) || ((e0 & (F | B)) && (e1 & (L | R))) };
		eo[i] = good ? 0 : 1;
	}
}

std::vector<uint8_t>& Solver::getDistCorner() { static std::vector<uint8_t> v; return v; }
std::vector<uint8_t>& Solver::getDistEdge1() { static std::vector<uint8_t> v; return v; }
std::vector<uint8_t>& Solver::getDistEdge2() { static std::vector<uint8_t> v; return v; }

const std::vector<Move>& Solver::getAllMoves() {
	static const std::vector<Move> v{
		U, U2, U3, R, R2, R3,
		F, F2, F3, D, D2, D3,
		L, L2, L3, B, B2, B3
	};
	return v;
}

uint32_t Solver::idCorner(const SearchState& s) {
	uint32_t rank{ 0 }, used{ 0 };
	for (int i{ 0 }; i < 7; ++i) {
		uint32_t count{ 0 };
		for (int j{ 0 }; j < s.cp[i]; ++j) if (!(used & (1 << j))) count++;
		rank += count * FACT[7 - i];
		used |= (1 << s.cp[i]);
	}
	uint32_t ori{ 0 };
	for (int i{ 0 }; i < 7; ++i) ori = ori * 3 + s.co[i];
	return rank * 2187 + ori;
}

static uint32_t getEdge6Index(const SearchState& s, const std::array<int, 6>& edgeSet) {
	uint32_t rank{ 0 }, used{ 0 };
	int pos[6]{}, ori[6]{};

	for (int i{ 0 }; i < 6; ++i) {
		for (int j{ 0 }; j < 12; ++j) {
			if (s.ep[j] == edgeSet[i]) {
				pos[i] = j;
				ori[i] = s.eo[j];
				break;
			}
		}
	}
	for (int i{ 0 }; i < 6; ++i) {
		uint32_t count{ 0 };
		for (int j{ 0 }; j < pos[i]; ++j) if (!(used & (1 << j))) count++;
		rank += count * (FACT[11 - i] / FACT[6]);
		used |= (1 << pos[i]);
	}
	uint32_t o{ 0 };
	for (int i{ 0 }; i < 6; ++i) o = (o << 1) | ori[i];
	return rank * 64 + o;
}

uint32_t Solver::idEdge1(const SearchState& s) { return getEdge6Index(s, EDGE_SET_1); }
uint32_t Solver::idEdge2(const SearchState& s) { return getEdge6Index(s, EDGE_SET_2); }

static int getHeuristic(const SearchState& s) {
    int hCorner = ptrCorner[Solver::idCorner(s)];
    int hEdge1  = ptrEdge1[Solver::idEdge1(s)];
    int hEdge2  = ptrEdge2[Solver::idEdge2(s)];
    return std::max({hCorner, hEdge1, hEdge2});
}

static bool dfsOptimal(const SearchState& cur, int g, int bound, Move lastMove,
	int& nextBound, std::vector<Move>& path, std::atomic<bool>& stopSignal) {
	if (stopSignal.load(std::memory_order_relaxed)) return false;

	int h{ getHeuristic(cur) };
	int f{ g + h };

	if (f > bound) {
		if (f < nextBound) nextBound = f;
		return false;
	}
	if (h == 0 && g > 0) return true;

	for (Move m : Solver::getAllMoves()) {
		if (lastMove != NONE) {
			int fL{ static_cast<int>(lastMove) / 3 };
			int fM{ static_cast<int>(m) / 3 };

			if (fL == fM) continue;
			if (fM == fL - 3) continue;
		}

		SearchState nextState{ cur };
		nextState.apply(m);
		path.push_back(m);

		if (dfsOptimal(nextState, g + 1, bound, m, nextBound, path, stopSignal)) {
			return true;
		}
		path.pop_back();
	}
	return false;
}

std::vector<Move> Solver::solveOptimal(const Cube& cube) {
	SearchState startState{};
	startState.isOptimal = true;
	startState.fromCube(cube);

	int bound{ getHeuristic(startState) };
	LOG("Initial Heuristic Bound: {}\n", bound);

	if (bound == 0) return {};

	std::vector<Move> bestSolution{};
	std::atomic<bool> found{ false };

	int numThreads{ static_cast<int>(std::thread::hardware_concurrency()) };
	if (numThreads == 0) numThreads = 8;

	while (bound <= 20) {
		LOG("Searching depth: {}\n", bound);
		int nextBound{ 999999 };
		std::mutex boundMutex{};

		struct SearchTask {
			SearchState state;
			std::vector<Move> path;
			Move lastMove;
		};
		std::vector<SearchTask> tasks{};

		if (bound <= 2) {
			for (Move m : getAllMoves()) {
				SearchState s{ startState };
				s.apply(m);
				tasks.push_back({ s, {m}, m });
			}
		}
		else {
			for (Move m1 : getAllMoves()) {
				SearchState s1{ startState };
				s1.apply(m1);
				int f1{ static_cast<int>(m1) / 3 };

				for (Move m2 : getAllMoves()) {
					int f2{ static_cast<int>(m2) / 3 };
					if (f1 == f2) continue;
					if (f2 == f1 - 3) continue;

					SearchState s2{ s1 };
					s2.apply(m2);
					tasks.push_back({ s2, {m1, m2}, m2 });
				}
			}
		}

		std::atomic<size_t> taskIndex{ 0 };
		std::vector<std::future<void>> futures{};

		for (int t = 0; t < numThreads; ++t) {
			futures.push_back(std::async(std::launch::async, [&]() {
				int localNextBound{ 999999 };

				while (!found.load(std::memory_order_relaxed)) {
					size_t idx = taskIndex.fetch_add(1, std::memory_order_relaxed);
					if (idx >= tasks.size()) break;

					const SearchTask& task = tasks[idx];
					std::vector<Move> localPath = task.path;

					if (dfsOptimal(task.state, task.path.size(), bound, task.lastMove, localNextBound, localPath, found)) {
						std::lock_guard<std::mutex> lock{ boundMutex };
						if (!found.exchange(true)) {
							bestSolution = localPath;
						}
						break;
					}
				}

				std::lock_guard<std::mutex> lock{ boundMutex };
				if (localNextBound < nextBound) {
					nextBound = localNextBound;
				}
				}));
		}

		for (auto& f : futures) f.wait();

		if (found) {
			constexpr std::array<const char*, 18> moveNames{
				"U", "U2", "U3", "R", "R2", "R3", "F", "F2", "F3",
				"D", "D2", "D3", "L", "L2", "L3", "B", "B2", "B3"
			};
			LOG("Optimal solution found! {} moves:\n", bestSolution.size());
			for (const auto& m : bestSolution) LOG("{} ", moveNames[static_cast<int>(m)]);
			LOG("\n");
			return bestSolution;
		}

		bound = nextBound;
	}

	return bestSolution;
}





void SearchState::moveE1ToE2(int E1, int E2)
{
	switch (E2)
	{
	case 1:
		switch (E1)
		{
		case 0:
			apply(U);
			break;
		case 2:
			apply(U3);
			break;
		case 3:
			apply(U2);
			break;
		case 4:
			apply({ R2, U, R2 });
			break;
		case 6:
			apply({ L2, U3, L2 });
			break;
		case 7:
			apply({ B2, U2 });
			break;
		case 10:
			apply({ R3, U, R });
			break;
		case 11:
			apply({ L, U3, L3 });
			break;
		}
		break;
	case 8:
		switch (E1)
		{
		case 0:
			apply(R3);
			break;
		case 2:
			apply({ U2, R3, U2 });
			break;
		case 3:
			apply({ U, R3, U3 });
			break;
		case 4:
			apply(R);
			break;
		case 6:
			apply({ D2, R, D2 });
			break;
		case 7:
			apply({ D3, R, D });
			break;
		case 10:
			apply({ B2, R2 });
			break;
		case 11:
			apply(R2);
			break;
		}
		break;
	case 5:
		switch (E1)
		{
		case 0:
			apply({ R2, D3, R2 });
			break;
		case 2:
			apply({ L2, D, L2 });
			break;
		case 3:
			apply({ B2, D2 });
			break;
		case 4:
			apply(D3);
			break;
		case 6:
			apply(D);
			break;
		case 7:
			apply(D2);
			break;
		case 10:
			apply({ L3, D, L });
			break;
		case 11:
			apply({ R, D3, R3 });
			break;
		}
		break;
	case 9:
		switch (E1)
		{
		case 0:
			apply({ U2, L, U2 });
			break;
		case 2:
			apply(L);
			break;
		case 3:
			apply({ U3, L, U });
			break;
		case 4:
			apply({ D2, L3, D2 });
			break;
		case 6:
			apply(L3);
			break;
		case 7:
			apply({ D, L3, D3 });
			break;
		case 10:
			apply(L2);
			break;
		case 11:
			apply({ B2, L2 });
			break;
		}
		break;
	}
}

int SearchState::findErrorEO()
{
	int EOIdx[]{ 0, 2, 3, 4, 6, 7, 10, 11 };
	for (int i{ 0 }; i < 8; ++i)
		if (eo[EOIdx[i]] == 1) return EOIdx[i];
	return -1;
}

void SearchState::handleLeft2ErrorEO(int hash)
{
	
	switch (hash)
	{
	case 1:
		apply({ U3, F3, U, F });
		break;
	case 2:
		apply({ D, R });
		handleLeft2ErrorEO();
		break;
	case 3:
		apply({ D, F, D3, F3 });
		break;
	case 4:
		apply({ U, F, U3, F3 });
		break;
	case 5:
		apply({ L3, U3 });
		handleLeft2ErrorEO();
		break;
	case 6:
		apply({ D3, F3, D, F });
		break;
	}
}


void SearchState::repositionE(int E, bool toU)
{
	if (toU)
	{
		while (ep[1] < 8) apply(U);
		switch (E)
		{
		case 8:	
			apply({ R, U3, R3 });
			break;
		case 9:
			apply({ L3, U, L });
			break;
		case 10:
			apply({ L, U, L3 });
			break;
		case 11:
			apply({ R3, U3, R });
			break;
		}
	}
	else
	{
		while (ep[5] < 8) apply(D);
		switch (E)
		{
		case 8:
			apply({ R3, D, R });
			break;
		case 9:
			apply({ L, D3, L3 });
			break;
		case 10:
			apply({L3, D3, L });
			break;
		case 11:
			apply({ R, D, R3 });
			break;
		}
	}
}

std::vector<Move> Solver::solve(const Cube& cube)
{
	SearchState ss{};
	ss.fromCube(cube);

	// Step 1: Orient the edges
	int F_EIdx[]{ 1, 8, 5, 9 };
	int errorEONum{ 0 };
	for (int i{ 0 }; i < 12; ++i) errorEONum += ss.eo[i];
	while (errorEONum > 0)
	{
		int errorEOPos{ ss.findErrorEO() };
		for (int i{ 0 }; i < 4; ++i)
		{
			if (errorEOPos != -1)
			{
				if (ss.eo[F_EIdx[i]] == 1) continue;
				ss.moveE1ToE2(errorEOPos, F_EIdx[i]);
				errorEOPos = ss.findErrorEO();
			}
			else if (errorEONum == 2)
			{
				int EOIdx[]{ 1, 8, 5, 9 };
				int hash[]{ 0, 1, 2, 4 };
				int totalHash{ 0 };
				for (int i{ 0 }; i < 4; ++i)
					if (ss.eo[EOIdx[i]] == 1) totalHash += hash[i];
				ss.handleLeft2ErrorEO(totalHash);
				errorEONum = 0;
				break;
			}
		}
		if (errorEONum > 0)
		{
			ss.apply(F);
			errorEONum -= 4;
		}
	}

	// Step 2: Separate the top and bottom from middle layer edges
	int U_capacity{ 0 };
	for (int i{ 0 }; i < 4; ++i) U_capacity += ss.ep[i] / 8;
	for (int i{ 8 }; i < 12; ++i)
	{
		if (ss.ep[i] < 8) ss.repositionE(i, U_capacity-- > 0);
	}

	// Step 3: Oreint the corners
	int U_errorCO{ 0 };
	int D_errorCO{ 0 };
	for (int i{ 0 }; i < 4; ++i)
	{
		U_errorCO += ss.co[i];
		D_errorCO += ss.co[i + 4];
	}
	if (U_errorCO % 3 > 0)
	{
		while ((ss.co[1] + ss.co[2] + ss.co[4] + ss.co[7]) % 3 > 0) ss.apply(D);
		U_errorCO += ss.co[4] + ss.co[7] - ss.co[0] - ss.co[3];
		D_errorCO += ss.co[0] + ss.co[3] - ss.co[4] - ss.co[7];
		ss.apply(R2);
	}
	while (U_errorCO > 0)
	{
		while (ss.co[0] == 0) ss.apply(U);
		switch (ss.co[0])
		{
		case 1:
			ss.apply({ R3, D3, R, D, R3, D3, R, D });
			U_errorCO -= 1;
			break;
		case 2:
			ss.apply({ D3, R3, D, R, D3, R3, D, R });
			U_errorCO -= 2;
			break;
		}
	}
	while (D_errorCO > 0)
	{
		while (ss.co[4] == 0) ss.apply(D);
		switch (ss.co[4])
		{
		case 1:
			ss.apply({ U, R, U3, R3, U, R, U3, R3 });
			D_errorCO -= 1;
			break;
		case 2:
			ss.apply({ R, U, R3, U3, R, U, R3, U3 });
			D_errorCO -= 2;
			break;
		}
	}

	// Step 4: Separate the top from bottom layer corners
	auto reposition
	{
		[&ss](auto& self, int DCinUNum) -> void
		{
			switch (DCinUNum)
			{
			case 1:
				while (ss.cp[0] < 4) ss.apply(U);
				while (ss.cp[5] >= 4) ss.apply(D);
				ss.apply({ R2, D, R2 });
				break;
			case 2:
			{
				bool U_cross{ ss.cp[0] / 4 == ss.cp[2] / 4 };
				bool D_cross{ ss.cp[4] / 4 == ss.cp[6] / 4 };
				if (U_cross)
				{
					if (D_cross)
					{
						if (ss.cp[0] < 4) ss.apply(U);
						if (ss.cp[4] < 4) ss.apply(D);
						ss.apply({ R2, U, D, R2 });
					}
					else
					{
						while (ss.cp[4] >= 4 || ss.cp[7] >= 4) ss.apply(D);
						ss.apply(R2);
						self(self, 1);
					}
				}
				else
				{
					if (D_cross)
					{
						while (ss.cp[0] < 4 || ss.cp[3] < 4) ss.apply(U);
						ss.apply(R2);
						self(self, 1);
					}
					else
					{
						while (ss.cp[0] < 4 || ss.cp[3] < 4) ss.apply(U);
						while (ss.cp[4] >= 4 || ss.cp[7] >= 4) ss.apply(D);
						ss.apply(R2);
					}
				}
				break;
			}
			case 3:
				while (ss.cp[0] < 4 || ss.cp[3] < 4) ss.apply(U);
				while (ss.cp[4] >= 4 || ss.cp[7] >= 4) ss.apply(D);
				ss.apply(R2);
				self(self, 1);
				break;
			case 4:
				ss.apply({ R2, L2 });
				break;
			}
		}
	};
	int DCinUNum{ 0 };
	for (int i{ 0 }; i < 4; ++i) DCinUNum += ss.cp[i] / 4;
	reposition(reposition, DCinUNum);

	// step 5: Permute the corners
	int U_headlights{ 0 };
	int D_headlights{ 0 };
	for (int i{ 0 }; i < 4; ++i)
	{
		if ((ss.cp[(i + 1) % 4] - ss.cp[i] + 4) % 4 == 1) ++U_headlights;
		if ((ss.cp[(i + 1) % 4 + 4] - ss.cp[i + 4] + 4) % 4 == 1) ++D_headlights;
	}
	std::setlocale(LC_ALL, ".UTF8");
	switch (U_headlights)
	{
	case 0:
		ss.apply({ F, R, U3, R3, U3, R, U, R3, F3, R, U, R3, U3, R3, F, R, F3 });
		break;
	case 1:
		while ((ss.cp[2] - ss.cp[1] + 4) % 4 != 1) ss.apply(U);
		ss.apply({ R, U, R3, U3, R3, F, R2, U3, R3, U3, R, U, R3, F3 });
		break;
	}
	switch (D_headlights)
	{
	case 0:
		ss.apply({ B, R, D3, R3, D3, R, D, R3, B3, R, D, R3, D3, R3, B, R, B3 });
		break;
	case 1:
		while ((ss.cp[6] - ss.cp[5] + 4) % 4 != 1) ss.apply(D);
		ss.apply({ R, D, R3, D3, R3, B, R2, D3, R3, D3, R, D, R3, B3 });
		break;
	}
	while (ss.cp[0] != 0) ss.apply(U);
	while (ss.cp[4] != 4) ss.apply(D);

	// step 6: Reposition the middle layer edges
	array middleLayerTriangleSwap
	{
		vector{U2, L2, U2, B2},
		vector{B2, D2, R2, D2},
		vector{R2, D2, F2, D2},
		vector{U2, F2, U2, L2}
	};
	int keepPos{ -1 };
	do
	{
		if (ss.ep[8] == 8 && ss.ep[9] == 9) break;
		for (int i{ 8 }; i < 12; ++i)
		{
			if (ss.ep[i] == i) keepPos = i - 8;
		}
		if (keepPos == -1) ss.apply(middleLayerTriangleSwap[0]);
		else
		{
			auto triangleSwap{ middleLayerTriangleSwap[keepPos] };
			if (ss.ep[(keepPos - 1) % 4 + 8] == (keepPos - 2) % 4 + 8)
			std::reverse(triangleSwap.begin(), triangleSwap.end());
			ss.apply(triangleSwap);
		}
	}
	while (keepPos == -1);

	// step 6: Reposition the top and bottom layer edge pieces
	// 对边换(H - Perm):
	// apply({ R2, U2, R, U2, R2, U2, R2, U2, R, U2, R2 });

	// 邻边换(Z - Perm):
	// apply({ R, U, R3, U, R3, U3, R3, U, R, U3, R3, U3, R2, U, R, U2 });
	int U_EinAdj{ 0 };
	int D_EinAdj{ 0 };
	for (int i{ 0 }; i < 4; ++i)
	{
		if (ss.ep[i] != i && ss.ep[i] != (i + 2) % 4) ++U_EinAdj;
		if (ss.ep[i + 4] != i + 4 && ss.ep[i + 4] != (i + 2) % 4 + 4) ++D_EinAdj;
	}


	// step 8: Permute the edges

	return ss.path;
}