#pragma once
#include <vector>
#include <cstdint>

using std::vector;

struct Cube;

enum Move : uint8_t
{
	U, U2, U3, R, R2, R3, F, F2, F3,
	D, D2, D3, L, L2, L3, B, B2, B3,
	NONE = 99
};

struct SearchState
{
	uint8_t co[ 8]{};
	uint8_t cp[ 8]{};
	uint8_t eo[12]{};
	uint8_t ep[12]{};
	bool isOptimal{ false };

	vector<Move> path{};
	void apply(Move m, bool rorateOnly = false);
	void apply(const vector<Move>& moves);
	void fromCube(const Cube& cube);
	void moveE1ToE2(int E1, int E2);
	int findErrorEO();
	void handleLeft2ErrorEO(int hash = 1);

	void repositionE(int E, bool toU);
};

class Solver
{
public:
	Solver();
	vector<Move> solve(const Cube& cube);
	vector<Move> solveOptimal(const Cube& cube);

	static void initMoveTables();

	static void generatePruneTables();
	static bool loadPruneTables();
	static void savePruneTables();

	static vector<uint8_t>& getDistCorner();
	static vector<uint8_t>& getDistEdge1();
	static vector<uint8_t>& getDistEdge2();

	static uint32_t idCorner(const SearchState& s);
	static uint32_t idEdge1(const SearchState& s);
	static uint32_t idEdge2(const SearchState& s);

	static uint8_t cpMove[18][8], coMove[18][8];
	static uint8_t epMove[18][12], eoMove[18][12];

	static const vector<Move>& getAllMoves();
};