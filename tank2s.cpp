#include <queue>
#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "jsoncpp/json.h"

using namespace std;

#define fieldWidth 9
#define fieldHeight 9
#define sideCount 2
#define tankPerSide 2
using pii = typename std::pair<int, int>;
clock_t ticker = clock();
const double INF=1e100;
typedef int Action;

struct DisappearLog
{
    int item;

    // 导致其消失的回合的编号
    int turn;

    int x, y;
    bool operator< (const DisappearLog& b) const
    {
        if (x == b.x)
        {
            if (y == b.y)
                return item < b.item;
            return y < b.y;
        }
        return x < b.x;
    }
};

namespace TankGame
{
	    enum GameResult
    {
        NotFinished = -2,
        Draw = -1,
        Blue = 0,
        Red = 1
    };

    template<typename T> inline T operator~ (T a) { return (T)~(int)a; }
    template<typename T> inline T operator| (T a, T b) { return (T)((int)a | (int)b); }
    template<typename T> inline T operator& (T a, T b) { return (T)((int)a & (int)b); }
    template<typename T> inline T operator^ (T a, T b) { return (T)((int)a ^ (int)b); }
    template<typename T> inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); }
    template<typename T> inline T& operator&= (T& a, T b) { return (T&)((int&)a &= (int)b); }
    template<typename T> inline T& operator^= (T& a, T b) { return (T&)((int&)a ^= (int)b); }

    const Action Invalid = -2, Stay = -1, Up = 0, Right = 1, Down = 2, Left = 3, UpShoot = 4, RightShoot = 5, DownShoot = 6, LeftShoot = 7;

	const int None = 0, Brick = 1,Steel = 2,Base = 4,Blue0 = 8,Blue1 = 16,Red0 = 32,Red1 = 64,Water = 128, Forest = 256;
	const char fieldTypes[4][12] = {
		"brickfield", "forestfield", "steelfield", "waterfield"
	};
	const int baseX[2] = { 4, 4 };
	const int baseY[2] = { 0, 8 };
	int dx[4] = { 0,1,0,-1 };
	int dy[4] = { -1,0,1,0 };
	class gameState
	{
	public:
		int state[10][10];
		std::vector< pii >  tank[2][2];
		bool lastShoot[2][2];
		bool certain[2][2];
		int mySide;
		int tankNum[2];
		bool tankAlive[2][2];
		bool baseAlive[2];
		pii lastAction;
		int todoActions[2][2];
		double stateScore = 0;
		gameState() {
			memset(state, 0, sizeof(state));
			memset(lastShoot, 0, sizeof(lastShoot));
			mySide = 0;
			for (int i = 0; i<2; ++i) for (int j = 0; j<2; ++j)
				tank[i][j].clear();
			state[0][4] = state[8][4] = Steel;
			baseAlive[0] = baseAlive[1] = 1;
			tankNum[0] = tankNum[1] = 2;
			for (int i = 0; i<2; ++i) for (int j = 0; j<2; ++j)
				tankAlive[i][j] = certain[i][j] = 1;
			tank[0][0].push_back(pii(0, 2));
			tank[0][1].push_back(pii(0, 6));
			tank[1][0].push_back(pii(8, 6));
			tank[1][1].push_back(pii(8, 2));


		}
		gameState(const gameState & ob)
		{
			memcpy(state, ob.state, sizeof(ob.state));
			memcpy(lastShoot, ob.lastShoot, sizeof(ob.lastShoot));
			for (int i = 0; i<2; ++i) for (int j = 0; j<2; ++j)
				tank[i][j] = ob.tank[i][j];
			for (int i = 0; i<2; ++i) tankNum[i] = ob.tankNum[i];
			mySide = ob.mySide;
			stateScore = ob.stateScore;
		}
		~gameState() = default;
		bool CoordValid(const int &x, const int &y)
		{
			return x >= 0 && x <= 8 && y >= 0 && y <= 8
				&& (~state[y][x] & Steel)
				&& (~state[y][x] & Water)
				&& (~state[y][x] & Brick);
		}

		int moveValid(int side, int id, int action)
		{
			if (tank[side][id].size() > 1)
				return -1;
			if (action == -1) return 1;
			if (action >= 4 && !lastShoot[side][id])
				return 1;
			int xx = tank[side][id][0].second, yy = tank[side][id][0].first;
			xx += dx[action], yy += dy[action];
			if (!CoordValid(xx, yy)) return 0;
			for (int s = 0; s < 2; ++s)
				for (int i = 0; i < 2; ++i)
					if (tank[s][i].size() == 1)
					{
						if (
							(tank[s][i][0].first == yy)
							&& (tank[s][i][0].second == xx)
							&& !(s != side && state[yy][xx] == Forest))
							return 0;
					}
			return 1;
		}
		        int gameField[fieldHeight][fieldWidth] = {};

        // 坦克横坐标，-1表示坦克已炸
        int tankX[sideCount][tankPerSide] = {
            { fieldWidth / 2 - 2, fieldWidth / 2 + 2 },{ fieldWidth / 2 + 2, fieldWidth / 2 - 2 }
        };

        // 坦克纵坐标，-1表示坦克已炸
        int tankY[sideCount][tankPerSide] = { { 0, 0 },{ fieldHeight - 1, fieldHeight - 1 } };

        // 当前回合编号
        int currentTurn = 1;

        // 用于回退的log
        stack<DisappearLog> logs;

        // 过往动作（previousActions[x] 表示所有人在第 x 回合的动作，第 0 回合的动作没有意义）
        Action previousActions[101][sideCount][tankPerSide] = { { { Stay, Stay },{ Stay, Stay } } };

        //!//!//!// 以上变量设计为只读，不推荐进行修改 //!//!//!//

        // 本回合双方即将执行的动作，需要手动填入
        Action nextAction[sideCount][tankPerSide] = { { Invalid, Invalid },{ Invalid, Invalid } };
        const int tankItemTypes[sideCount][tankPerSide] = {
	        { Blue0, Blue1 },{ Red0, Red1 }
	    };

        // 判断行为是否合法（出界或移动到非空格子算作非法）
        // 未考虑坦克是否存活

        // 判断 nextAction 中的所有行为是否都合法
        // 忽略掉未存活的坦克
        bool ActionIsValid()
        {
            for (int side = 0; side < sideCount; side++)
                for (int tank = 0; tank < tankPerSide; tank++)
                    if (tankAlive[side][tank] && !moveValid(side, tank, nextAction[side][tank]))
                        return false;
            return true;
        }

    private:
    	inline bool ActionIsMove(Action x)
	    {
	        return x >= Up && x <= Left;
	    }

	    inline bool ActionIsShoot(Action x)
	    {
	        return x >= UpShoot && x <= LeftShoot;
	    }

	    inline bool ActionDirectionIsOpposite(Action a, Action b)
	    {
	        return a >= Up && b >= Up && (a + 2) % 4 == b % 4;
	    }
	    inline int GetTankSide(int item)
	    {
	        return item == Blue0 || item == Blue1 ? Blue : Red;
	    }

	    inline int GetTankID(int item)
	    {
	        return item == Blue0 || item == Red0 ? 0 : 1;
	    }
	    inline bool HasMultipleTank(int item)
	    {
	        // 如果格子上只有一个物件，那么 item 的值是 2 的幂或 0
	        // 对于数字 x，x & (x - 1) == 0 当且仅当 x 是 2 的幂或 0
	        return !!(item & (item - 1));
	    }
	    // 获得动作的方向
	    inline int ExtractDirectionFromAction(Action x)
	    {
	        if (x >= Up)
	            return x % 4;
	        return -1;
	    }
        void _destroyTank(int side, int tank)
        {
            tankAlive[side][tank] = false;
            tankX[side][tank] = tankY[side][tank] = -1;
        }

        void _revertTank(int side, int tank, DisappearLog& log)
        {
            int &currX = tankX[side][tank], &currY = tankY[side][tank];
            if (tankAlive[side][tank])
                gameField[currY][currX] &= ~tankItemTypes[side][tank];
            else
                tankAlive[side][tank] = true;
            currX = log.x;
            currY = log.y;
            gameField[currY][currX] |= tankItemTypes[side][tank];
        }
    public:

        // 执行 nextAction 中指定的行为并进入下一回合，返回行为是否合法
        bool DoAction()
        {
            if (!ActionIsValid())
                return false;

            //cout<<"ActionDid"<<nextAction[0][0]<<' '<<nextAction[0][1]<<' '<<nextAction[1][0]<<' '<<nextAction[1][1]<<endl;

            // 1 移动
            for (int side = 0; side < sideCount; side++)
                for (int tank = 0; tank < tankPerSide; tank++)
                {
                    Action act = nextAction[side][tank];

                    // 保存动作
                    previousActions[currentTurn][side][tank] = act;
                    if (tankAlive[side][tank] && ActionIsMove(act))
                    {
                        int &x = tankX[side][tank], &y = tankY[side][tank];
                        int &items = gameField[y][x];

                        // 记录 Log
                        DisappearLog log;
                        log.x = x;
                        log.y = y;
                        log.item = tankItemTypes[side][tank];
                        log.turn = currentTurn;
                        logs.push(log);

                        // 变更坐标
                        x += dx[act];
                        y += dy[act];

                        // 更换标记（注意格子可能有多个坦克）
                        gameField[y][x] |= log.item;
                        items &= ~log.item;
                    }
                }

            // 2 射♂击!
            set<DisappearLog> itemsToBeDestroyed;
            for (int side = 0; side < sideCount; side++)
                for (int tank = 0; tank < tankPerSide; tank++)
                {
                    Action act = nextAction[side][tank];
                    if (tankAlive[side][tank] && ActionIsShoot(act))
                    {
                        int dir = ExtractDirectionFromAction(act);
                        int x = tankX[side][tank], y = tankY[side][tank];
                        bool hasMultipleTankWithMe = HasMultipleTank(gameField[y][x]);
                        while (true)
                        {
                            x += dx[dir];
                            y += dy[dir];
                            if (!CoordValid(x, y))
                                break;
                            int items = gameField[y][x];
                            //tank will not be on water, and water will not be shot, so it can be handled as None
                            if (items != None && items != Water)
                            {
                                // 对射判断
                                if (items >= Blue0 &&
                                    !hasMultipleTankWithMe && !HasMultipleTank(items))
                                {
                                    // 自己这里和射到的目标格子都只有一个坦克
                                    Action theirAction = nextAction[GetTankSide(items)][GetTankID(items)];
                                    if (ActionIsShoot(theirAction) &&
                                        ActionDirectionIsOpposite(act, theirAction))
                                    {
                                        // 而且我方和对方的射击方向是反的
                                        // 那么就忽视这次射击
                                        break;
                                    }
                                }

                                // 标记这些物件要被摧毁了（防止重复摧毁）
                                for (int mask = 1; mask <= Red1; mask <<= 1)
                                    if (items & mask)
                                    {
                                        DisappearLog log;
                                        log.x = x;
                                        log.y = y;
                                        log.item = (int)mask;
                                        log.turn = currentTurn;
                                        itemsToBeDestroyed.insert(log);
                                    }
                                break;
                            }
                        }
                    }
                }

            for (auto& log : itemsToBeDestroyed)
            {
                switch (log.item)
                {
                case Base:
                {
                    int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
                    baseAlive[side] = false;
                    break;
                }
                case Blue0:
                    _destroyTank(Blue, 0);
                    break;
                case Blue1:
                    _destroyTank(Blue, 1);
                    break;
                case Red0:
                    _destroyTank(Red, 0);
                    break;
                case Red1:
                    _destroyTank(Red, 1);
                    break;
                case Steel:
                    continue;
                default:
                    ;
                }
                gameField[log.y][log.x] &= ~log.item;
                logs.push(log);
            }

            for (int side = 0; side < sideCount; side++)
                for (int tank = 0; tank < tankPerSide; tank++)
                    nextAction[side][tank] = Invalid;

            currentTurn++;
            return true;
        }

        // 回到上一回合
        bool Revert()
        {
            if (currentTurn == 1)
                return false;
            //cout<<"Undo"<<endl;
            currentTurn--;
            while (!logs.empty())
            {
                DisappearLog& log = logs.top();
                if (log.turn == currentTurn)
                {
                    logs.pop();
                    switch (log.item)
                    {
                    case Base:
                    {
                        int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
                        baseAlive[side] = true;
                        gameField[log.y][log.x] = Base;
                        break;
                    }
                    case Brick:
                        gameField[log.y][log.x] = Brick;
                        break;
                    case Blue0:
                        _revertTank(Blue, 0, log);
                        break;
                    case Blue1:
                        _revertTank(Blue, 1, log);
                        break;
                    case Red0:
                        _revertTank(Red, 0, log);
                        break;
                    case Red1:
                        _revertTank(Red, 1, log);
                        break;
                    default:
                        ;
                    }
                }
                else
                    break;
            }
            return true;
        }


		int sgn(int x) {
			return x > 0 ? 1 : (x == 0 ? 0 : -1);
		}
		bool beKilled(int y, int x)
		{
			bool Killed = 0;
			for (int i = 0; i<2; ++i)
			{
				if (!tankAlive[mySide][i])
					continue;
				int k;
				if (i == 0)
					if (lastAction.first < 4) continue;
					else k = lastAction.first;
				else
					if (lastAction.second < 4) continue;
					else k = lastAction.second;
					int yy = tank[mySide][i][0].first;
					int xx = tank[mySide][i][0].second;
					int deltaY = y - yy;
					int deltaX = x - xx;

					if (sgn(deltaY) == sgn(dy[k]) && sgn(deltaX) == sgn(dx[k]))
					{
						bool good = 1;
						for (int m = yy, n = xx; m != y || n != x; m += dy[k], n += dx[k])
						{
							if (state[m][n] != Forest && state[m][n] != None)
							{
								good = 0;
								break;
							}
						}
						Killed |= good;
					}
			}
			return Killed;
		}
		void expand(std::vector<pii>  &v)
		{
			// cout << "expanding: ";
			// for (auto x : v)
			//     cout << "( " << x.first << " , " << x.second << " )";
			// cout << endl;

			std::vector<pii> newV;
			newV.clear();
			bool vis[9][9] = { 0 };
			for (auto x : v)
				if (!vis[x.first][x.second])
				{
					vis[x.first][x.second] = 1;
					if ((state[x.first][x.second] & Forest)
						&& CoordValid(x.second, x.first)
						&& !beKilled(x.first, x.second))
						newV.push_back(x);
				}
			for (auto x : v)
			{
				for (int k = 0; k<4; ++k)
				{
					int yy = x.first + dy[k], xx = x.second + dx[k];
					if (CoordValid(xx, yy) && !vis[yy][xx])
					{
						// cout << "( " << yy << " , " << xx << " )" << endl;
						vis[yy][xx] = 1;
						if ((state[yy][xx] & Forest) && !beKilled(yy, xx))
							newV.push_back(pii(yy, xx));
					}
				}
			}
			v = newV;
		}
		void init()
		{
			Json::Reader reader;
			Json::Value input, temp, all;
			reader.parse(cin, all);
			input = all["requests"];
			temp = all["responses"];
			for (int i = 0; i < input.size(); ++i)
			{
				if (i == 0)
				{
					mySide = input[0u]["mySide"].asInt();
					for (int s = 0; s < 4; ++s)
						for (unsigned j = 0; j < 3; ++j)
						{
							int x = input[0u][fieldTypes[s]][j].asInt();
							for (int k = 0; k < 27; ++k)
								state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * (1 << s);
						}
				}
				else
				{
					lastShoot[1 - mySide][0] = lastShoot[1 - mySide][1] = 0;
					for (int j = 0; j<2; ++j)
						if (input[i]["actions"][j] == -1)
							certain[1 - mySide][j] = 1, tankAlive[1 - mySide][j] = 0;
						else
							if (input[i]["actions"][j] >= 4)
								lastShoot[1 - mySide][j] = 1;

					for (int j = 0; j<2; ++j)
						if (input[i]["final_enemy_positions"][j * 2].asInt() != -2)
						{
							tank[1 - mySide][j].clear();
							tankAlive[1 - mySide][j] = 1;
							if (input[i]["final_enemy_positions"][j * 2].asInt() != -1)
								tank[1 - mySide][j].push_back(
									pii(input[i]["final_enemy_positions"][j * 2 + 1].asInt()
										, input[i]["final_enemy_positions"][j * 2].asInt()
									));
							else
							{
								tankAlive[1 - mySide][j] = 0;
							}
							certain[1 - mySide][j] = 1;
						}
						else certain[1 - mySide][j] = 0;

						int cutDown = 0;
						bool beKilled[2] = { 0 };
						pii newPos[2];
						for (int j = 0; j<2; ++j)
						{
							if (!tankAlive[mySide][j])
								newPos[j].first = newPos[j].second = 0;
							else
							{
								int y = tank[mySide][j][0].first;
								int x = tank[mySide][j][0].second;
								// cout << "oldPos of " << j << " is ( " << y << " , " << x << " )" << endl;
								int k = temp[i - 1][j].asInt();
								newPos[j].first = y;
								newPos[j].second = x;
								if (k >= 4 || k == -1) continue;
								newPos[j].first += dy[k];
								newPos[j].second += dx[k];
								// cout << "newPos of " << j << " is ( " << newPos[j].first << " , " << newPos[j].second << " )" << endl;
							}

						}
						if (!(certain[1 - mySide][0] && certain[1 - mySide][1]))
						{
							for (int j = 0; j<input[i]["destroyed_tanks"].size(); j += 2)
							{
								int x = input[i]["destroyed_tanks"][j].asInt();
								int y = input[i]["destroyed_tanks"][j + 1].asInt();
								bool ofEnemy = 1;
								for (int k = 0; k<2; ++k)
									if (tankAlive[mySide][k]
										&& newPos[k].first == y
										&& newPos[k].second == x)
									{
										ofEnemy = 0;
										beKilled[k] = 1;
										break;
									}
								if (ofEnemy)
								{
									cutDown += (certain[1 - mySide][0] == 0) && (certain[1 - mySide][1] == 0);
									for (int k = 0; k<2; ++k)
										if (certain[1 - mySide][k] && tankAlive[1 - mySide][k])
										{
											if (tank[1 - mySide][k][0].first != y
												|| tank[1 - mySide][k][1].second != x)
											{
												tank[1 - mySide][1 - k].clear();
												tankAlive[1 - mySide][1 - k] = 0;
												certain[1 - mySide][1 - k] = 1;
											}
										}
								}
							}
						}
						tankNum[1 - mySide] = tankAlive[1 - mySide][0] + tankAlive[1 - mySide][1] - cutDown;
						if (cutDown == 2)
						{
							certain[1 - mySide][0] = certain[1 - mySide][1] = 1;
							tankAlive[1 - mySide][0] = tankAlive[1 - mySide][1] = 0;
						}
						for (int j = 0; j<input[i]["destroyed_blocks"].size(); j += 2)
						{
							int x = input[i]["destroyed_blocks"][j].asInt();
							int y = input[i]["destroyed_blocks"][j + 1].asInt();
							state[y][x] = 0;
						}
						for (int j = 0; j<2; ++j)
							baseAlive[j] = state[baseY[j]][baseX[j]] != None;

						int uw=0;
						lastAction = pii(temp[i - 1][uw].asInt(), temp[i - 1][uw+1].asInt());

						for (int j = 0; j<2; ++j)
							if (!certain[1 - mySide][j])
								expand(tank[1 - mySide][j]);
						for (int j = 0; j<2; ++j)
						{
							if (tankAlive[mySide][j] == 0)
								continue;
							int x = temp[i - 1][j].asInt();
							if (x >= 4) lastShoot[mySide][j] = 1;
							else lastShoot[mySide][j] = 0;
							tank[mySide][j][0] = newPos[j];
							if (beKilled[j])
								tankAlive[mySide][j] = 0;
							if (tankAlive[mySide][j] == 0)
								tank[mySide][j].clear();
						}
						tankNum[mySide] = tankAlive[mySide][0] + tankAlive[mySide][1];
				}
			}

		}

		double getResult()
		{
			if (baseAlive[0] && !baseAlive[1]) return 0;
			else if (baseAlive[1] && !baseAlive[0]) return 1;
			else if (tankNum[0] && !tankNum[1]) return 0;
			else if (tankNum[1] && !tankNum[0]) return 1;
			else if (!baseAlive[0] && !baseAlive[1]) return 0.5;
			else if (!tankNum[0] && !tankNum[1]) return 0.5;
			else return -1;
		}
		void debugPrint()
		{
			for (int i = 0; i<9; ++i)
			{
				for (int j = 0; j<9; ++j)
				{
					switch (state[i][j])
					{
					case None: putchar('.'); break;
					case Steel: putchar('%'); break;
					case Water: putchar('W'); break;
					case Brick: putchar('*'); break;
					case Forest: putchar('#'); break;
					default:
						break;
					}
				}
				puts("");
			}
			for (int i = 0; i<2; ++i)
			{
				cout << "Side " << i << " :" << endl;
				for (int j = 0; j<2; ++j)
				{
					cout << j << " is " << (tankAlive[i][j] ? ("Alive") : ("not Alive")) << " and possible positions: ";
					for (auto x : tank[i][j])
						cout << "( " << x.first << " , " << x.second << " ) " << "; ";
					cout << endl;
				}
				puts("");
			}
			for (int i = 0; i<2; ++i)
				cout << "number of tanks of " << i << " is " << tankNum[i] << endl;
			for (int i = 0; i<2; ++i)
				for (int j = 0; j<2; ++j)
					cout << "Tank " << j << " of Side " << i << (lastShoot[i][j] ? "shoot" : "not shoot") << " last turn" << endl;
			cout << "stateScore is " << stateScore << endl;
		}

	};

	int outputAction(int a, int b)
	{
		Json::Value output;
		output = Json::Value(Json::arrayValue);
		output.append(a);
		output.append(b);
		Json::FastWriter writer;
		cout << writer.write(output) << endl;
		return 0;
	}
}
struct node {
	int x, y;
	double length = 0;

};
bool inmap(int y, int x) {
	if (x >= 0 && x < 9 && y >= 0 && y < 9) {
		return true;
	}
	return false;
}
double totallength[9][9] = { 0 };
//先y后x
void BFS(int y, int x, const TankGame::gameState &a) {
	for (int i = 0; i < 9; i++) {
		for (int j = 0; j < 9; j++) {
			totallength[i][j] = 1000;
		}
	}
	queue <node> q;
	node temp;
	temp.x = x;
	temp.y = y;
	q.push(temp);
	int visit[9][9] = { 0 };
	while (!q.empty()) {
		node Now = q.front();
		int x1 = Now.x;
		int y1 = Now.y;
		visit[y1][x1] = 1;
		for (int i = 0; i < 4; i++) {
			int x2 = x1 + TankGame::dx[i];
			int y2 = y1 + TankGame::dy[i];
			if (!visit[y2][x2] && inmap(y2, x2) && a.state[y2][x2] == TankGame::None) {
				node Next;
				Next.x = x2;
				Next.y = y2;
				Next.length = Now.length + 1;
				if (Next.length < totallength[y2][x2]) {
					totallength[y2][x2] = Next.length;
				}
				q.push(Next);
			}
			else if (!visit[y2][x2] && inmap(y2, x2) && a.state[y2][x2] == TankGame::Brick) {
				node Next;
				Next.x = x2;
				Next.y = y2;
				Next.length = Now.length + 2;
				if (Next.length < totallength[y2][x2]) {
					totallength[y2][x2] = Next.length;
				}
				q.push(Next);
			}
			else if (!visit[y2][x2] && inmap(y2, x2) && a.state[y2][x2] == TankGame::Forest) {
				node Next;
				Next.x = x2;
				Next.y = y2;
				Next.length = Now.length + 0.5;
				if (Next.length < totallength[x2][y2]) {
					totallength[y2][x2] = Next.length;
				}
				q.push(Next);
			}

		}
		q.pop();
	}
	return;

}
int findbestway(int y, int x, int side, const TankGame::gameState &a) {
	BFS(y, x, a);
	int minlength = 1000;
	int temp = TankGame::baseY[1 - side];
	int cnt = 0;
	for (int i = 1; i <= 4; i++) {
		if (a.state[temp][4 + i] == TankGame::None || a.state[temp][4 + i] == TankGame::Brick || a.state[temp][4 + i] == TankGame::Water) {
			if (a.state[temp][4 + i] == TankGame::Brick) { cnt++; totallength[temp][4 + i] += cnt; }
			if (totallength[temp][4 + i] < minlength) {
				minlength = totallength[temp][4 + i];
			}
		}
		else {
			break;
		}
	}
	cnt = 0;
	for (int i = 1; i <= 4; i++) {
		if (a.state[temp][4 - i] == TankGame::None || a.state[temp][4 - i] == TankGame::Brick || a.state[temp][4 - i] == TankGame::Water) {
			if (a.state[temp][4 - i] == TankGame::Brick) { cnt++; totallength[temp][4 - i] += cnt; }
			if (totallength[temp][4 + i] < minlength) {
				minlength = totallength[temp][4 + i];
			}
		}
		else {
			break;
		}
	}
	cnt = 0;
	for (int i = 0; i <= 3; i++) {
		if (a.state[abs(temp - i)][4] == TankGame::None || a.state[abs(temp - i)][4] == TankGame::Brick || a.state[temp][4 - i] == TankGame::Water) {
			if (a.state[abs(temp - i)][4] == TankGame::Brick) { cnt++; totallength[abs(temp - i)][4] += cnt; }
			if (totallength[abs(temp - i)][4] < minlength) {
				minlength = totallength[abs(temp - i)][4];
			}
		}
		else {
			break;
		}
	}
	return minlength;
}
bool canbekilled(int side, int id, const TankGame::gameState &a) {
	for (int i = 0; i < 2; i++) {
		if (a.tank[side][id][0].first == a.tank[1 - side][i][0].first&&a.tank[side][id][0].second == a.tank[1 - side][1 - i][0].second) {
			if (!a.lastShoot[1 - side][i] && !a.lastShoot[1 - side][1 - i] && a.tankAlive[1 - side][i] && a.tankAlive[1 - side][1 - i]) {
				return true;
			}
		}
	}
	return false;
}
void statevalue(TankGame::gameState &a) {
	double sum = 0;
	if (!a.baseAlive[a.mySide] || a.tankNum[a.mySide] == 0) {
		a.stateScore = 100;
		return;
	}
	if (!a.baseAlive[1 - a.mySide] || a.tankNum[1 - a.mySide] == 0) {
		a.stateScore = -100;
		return;
	}
	if (a.tankNum[a.mySide]<2) {
		sum += 20;
	}
	if (a.tankNum[1 - a.mySide]<2) {
		sum -= 20;
	}
	for (int i = 0; i < 2; i++) {
		if (canbekilled(1 - a.mySide, i, a)) {
			sum += -20;
			break;
		}
	}
	for (int i = 0; i < 2; i++) {
		if (canbekilled(a.mySide, i, a)) {
			sum += 20;
			break;
		}
	}
	for (int i = 0; i < 2; i++) {
		if (a.tankAlive[a.mySide][i]) {
			sum += findbestway(a.tank[a.mySide][i][0].first, a.tank[a.mySide][i][0].second, a.mySide, a);
		}
	}
	for (int i = 0; i < 2; i++) {
		if (a.tankAlive[1 - a.mySide][i]) {
			sum -= 0.5* findbestway(a.tank[1 - a.mySide][i][0].first, a.tank[1 - a.mySide][i][0].second, 1 - a.mySide, a);
		}
	}

	a.stateScore = sum;
	return;
}

double value(TankGame::gameState &field){
	statevalue(field);
	return field.stateScore;
}

struct TwoActions{
    Action action1, action2;
    TwoActions():action1(TankGame::Stay),action2(action1){}
    TwoActions(const TwoActions &other):action1(other.action1),action2(other.action2){}
    TwoActions(Action a, Action b):action1(a),action2(b){}
    friend ostream& operator<<(ostream &out, TwoActions x){
        out<<'('<<x.action1<<','<<x.action2<<')';
        return out;
    }
};

//Version 1.1
class MyArtificialIdiot1{
public:
    const static int search_depth_limit = 2;
    static int gen(int k){return rand()%k;}
    /*
    pair<TwoActions, double> search1(TankGame::TankField &field, double (*value)(TankGame::TankField &f), double alpha, double beta, int depth){
        vector<TwoActions> myValidActions, enemyValidActions;
        vector<Action> my[2],enemy[2];
        int me=field.mySide;
        for(Action i=TankGame::Stay;i<=TankGame::LeftShoot;i = Action(i+1)){
            if(field.ActionIsValid(me,0,i))my[0].push_back(i);
            if(field.ActionIsValid(me,1,i))my[1].push_back(i);
            if(field.ActionIsValid(1-me,0,i))enemy[0].push_back(i);
            if(field.ActionIsValid(1-me,1,i))enemy[1].push_back(i);
        }
        for(int i=0;i<2;i++){
            if(!field.tankAlive[me][i]){
                my[i].clear();
                my[i].push_back(TankGame::Stay);
            }
            if(!field.tankAlive[1-me][i]){
                enemy[i].clear();
                enemy[i].push_back(TankGame::Stay);
            }
        }
        for(auto u:my[0]){
            for(auto v:my[1]){
                myValidActions.push_back(TwoActions(u,v));
            }
        }
        for(auto u:enemy[0]){
            for(auto v:enemy[1]){
                enemyValidActions.push_back(TwoActions(u,v));
            }
        }
        vector<TwoActions> response;
        double max_min_result = -INF;
        for(auto myAction:myValidActions){
            double current_min = INF;
            for(auto enemyAction:enemyValidActions){
                field.nextAction[me][0] = myAction.action1;
                field.nextAction[me][1] = myAction.action2;
                field.nextAction[1-me][0] = enemyAction.action1;
                field.nextAction[1-me][1] = enemyAction.action2;
                field.DoAction();
                double u=value(field);
                updmin(current_min, u);
                field.Revert();
                if(current_min < max_min_result || current_min < alpha)break;
            }
            if(current_min > max_min_result){
                max_min_result = current_min;
                response.clear();
                response.push_back(myAction);
            }
            else if(current_min == max_min_result)response.push_back(myAction);
            if(max_min_result > beta)break;
        }
        TwoActions ret_value=response[myRand(0,response.size()-1)];
        return pair<TwoActions, double>(ret_value, max_min_result);
    }*/
    double alpha_beta_search(TankGame::gameState &field, double (*value)(TankGame::gameState &f), double alpha, double beta, int depth, bool isenemy, TwoActions pre){
	    using namespace TankGame;
	    double result = field.getResult();
	    if(result!=-1)return value(field);
	    vector<TwoActions> myValidActions;
	    vector<Action> my[2];
	    int me=field.mySide;
	    if(isenemy)me = 1 - me;

	    for(Action i=TankGame::Stay;i<=TankGame::LeftShoot;i = Action(i+1)){
	        if(field.moveValid(me,0,i))my[0].push_back(i);
	        if(field.moveValid(me,1,i))my[1].push_back(i);
	    }
	    for(int i=0;i<2;i++){
            if(!field.tankAlive[me][i]){
                my[i].clear();
                my[i].push_back(TankGame::Stay);
            }
        }
        for(auto u:my[0]){
            for(auto v:my[1]){
                myValidActions.push_back(TwoActions(u,v));
            }
        }
        random_shuffle(myValidActions.begin(),myValidActions.end(),gen);
        for(auto action: myValidActions){
            if(isenemy){
                field.nextAction[me][0]=action.action1;
                field.nextAction[me][1]=action.action2;
                field.nextAction[1-me][0]=pre.action1;
                field.nextAction[1-me][1]=pre.action2;
                field.DoAction();
                double u = alpha_beta_search(field, value, alpha, beta, depth-1, false, TwoActions());
                if(beta > u)beta = u;
                field.Revert();
            }
            else{
                double u = alpha_beta_search(field, value, alpha, beta, depth, true, action);
                if(alpha < u)alpha = u;
            }
            if(alpha >= beta)break;
            if((clock()-ticker)/CLOCKS_PER_SEC > 0.90)break;
        }
        if(isenemy)return beta;
        else return alpha;
    }
    double search(TankGame::gameState &field, double (*value)(TankGame::gameState &f), TwoActions myChoice, pair<pii, pii> enemyPosition){
    	using namespace TankGame;
    	gameState newState(field);
    	int me = newState.mySide;
    	newState.tank[1-me][0].clear();
    	newState.tank[1-me][0].push_back(enemyPosition.first);
    	newState.tank[1-me][1].clear();
    	newState.tank[1-me][1].push_back(enemyPosition.second);
    	field.certain[1-me][0]=field.certain[1-me][1]=true;
    	return alpha_beta_search(newState, value, -INF, INF, search_depth_limit, true, myChoice);
    }
    TwoActions run(TankGame::gameState &field, double (*value)(TankGame::gameState &f)){
    	using namespace TankGame;
    	vector<int> validActions[2];
    	vector<TwoActions> choices;
    	vector<pair<pii, pii> >enemyPositions;
    	int me = field.mySide;
    	for(int i=-1;i<=7;i++){
    		if(field.moveValid(i, field.mySide, 0))validActions[0].push_back(i);
    		if(field.moveValid(i, field.mySide, 1))validActions[1].push_back(i);
    	}
    	for(auto u:validActions[0]){
    		for(auto v:validActions[1]){
    			choices.push_back(TwoActions(u,v));
    		}
    	}
    	for(auto u:field.tank[1-me][0]){
    		for(auto v:field.tank[1-me][1]){
    			enemyPositions.push_back(make_pair(u,v));
    		}
    	}
    	double mini=INF;
    	TwoActions response;
    	for(auto myChoice:choices){
    		double cur=0;
    		for(auto enemyPosition:enemyPositions){
    			double thisState = search(field, value, myChoice, enemyPosition);
    			cur += exp(thisState/10);
    		}
    		if(cur<mini){
    			mini=cur;
    			response = myChoice;
    		}
    	}
        return response;
    }
};

int main()
{
	freopen("input.txt", "r", stdin);
	freopen("output.txt", "w", stdout);

	TankGame::gameState  state;
	state.init();
	//state.debugPrint();
	MyArtificialIdiot1 myAI;
	TwoActions reply = myAI.run(state, value);
	TankGame::outputAction(reply.action1, reply.action2);
	return 0;

}