/**
 * Framework for Threes! and its variants (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include <vector>

#include "board.h"
#include "action.h"
#include "weight.h"
#include "board.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * base agent for agents with weight tables and a learning rate
 */
class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args), alpha(0) {
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end())
			save_weights(meta["save"]);
	}

protected:
	virtual void init_weights(const std::string& info) {
		std::string res = info; // comma-separated sizes, e.g., "65536,65536"
		for (char& ch : res)
			if (!std::isdigit(ch)) ch = ' ';
		std::stringstream in(res);
		for (size_t size; in >> size; net.emplace_back(size));
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	std::vector<weight> net;
	float alpha;
};




/**
 * default random environment, i.e., placer
 * place the hint tile and decide a new hint tile
 */
class random_placer : public random_agent {
public:
	random_placer(const std::string& args = "") : random_agent("name=slide role=slider " + args) {
		spaces[0] = { 12, 13, 14, 15 };
		spaces[1] = { 0, 4, 8, 12 };
		spaces[2] = { 0, 1, 2, 3};
		spaces[3] = { 3, 7, 11, 15 };
		spaces[4] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	}

	virtual action take_action(const board& after) {
		std::vector<int> space = spaces[after.last()];
		std::shuffle(space.begin(), space.end(), engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;

			int bag[3], num = 0;
			for (board::cell t = 1; t <= 3; t++)
				for (size_t i = 0; i < after.bag(t); i++)
					bag[num++] = t;
			std::shuffle(bag, bag + num, engine);

			board::cell tile = after.hint() ?: bag[--num];
			board::cell hint = bag[--num];

			return action::place(pos, tile, hint);
		}
		return action();
	}

private:
	std::vector<int> spaces[5];
};

/**
 * random player, i.e., slider
 * select a legal action randomly
 */
class random_slider : public random_agent {
public:
	random_slider(const std::string& args = "") : random_agent("name=slide role=slider " + args),
		opcode({ 0, 1, 2, 3 }) {}

	virtual action take_action(const board& before) {
		std::shuffle(opcode.begin(), opcode.end(), engine);
		for (int op : opcode) {
			board::reward reward = board(before).slide(op);
			if (reward != -1) return action::slide(op);
		}
		return action();
	}

private:
	std::array<int, 4> opcode;
};

/*

	N-TUPLE

*/

class tuple_agent : public weight_agent{
public:
	tuple_agent(const std::string& args = "") : weight_agent("name=slide role=slider" + args) {
		prev = NULL; 
		current = NULL;
	}

	void put_board(board* b){
		if(prev == NULL)	prev = b;
		else{
			prev = current;
			current = b;
			
			update();	
		}
	}

	void update(){
		float prev_score = prev->value(); 
		
		std::cout << "GET ALL VALUE\n";
		float current_score = current->value();
		float prev_value = 0, current_value = 0;
		for(int i = 0; i < 4; i++){
			prev_value += net[i][index_to_index((*prev)[0][i], (*prev)[1][i], (*prev)[2][i], (*prev)[3][i])];
			current_value += net[i][index_to_index((*current)[0][i], (*current)[1][i], (*current)[2][i], (*current)[3][i])];
		}
		std::cout << "GET ALL VALUE\n";
		for(int i = 0; i < 4; i++){
			prev_value += net[i][index_to_index((*prev)[i][0], (*prev)[i][1], (*prev)[i][2], (*prev)[i][3])];
			current_value += net[i][index_to_index((*current)[i][0], (*current)[i][1], (*current)[i][2], (*current)[i][3])];
		}
		
		for(int i = 0; i < 4; i++){
			unsigned long long prev_index = index_to_index((*prev)[0][i], (*prev)[1][i], (*prev)[2][i], (*prev)[3][i]);
			net[i][prev_index] = net[i][prev_index] + alpha * ( (current_score - prev_score) +  current_value - prev_value);
		}
		for(int i = 0; i < 4; i++){
			unsigned long long prev_index = index_to_index((*prev)[i][0], (*prev)[i][1], (*prev)[i][2], (*prev)[i][3]);
			net[i + 4][prev_index] = net[i + 4][prev_index] + alpha * ( (current_score - prev_score) +  current_value - prev_value);
		}
	}

	void last_update(){
		float prev_value = 0;
		for(int i = 0; i < 4; i++) prev_value += net[i][index_to_index((*prev)[0][i], (*prev)[1][i], (*prev)[2][i], (*prev)[3][i])];
		for(int i = 4; i < 8; i++) prev_value += net[i][index_to_index((*prev)[i][0], (*prev)[i][1], (*prev)[i][2], (*prev)[i][3])];

		for(int i = 0; i < 4; i++){
			unsigned long long prev_index = index_to_index((*prev)[0][i], (*prev)[1][i], (*prev)[2][i], (*prev)[3][i]);
			net[i][prev_index] = net[i][prev_index] + alpha * ( 0 - prev_value);
		}
		for(int i = 4; i < 8; i++){
			unsigned long long prev_index = index_to_index((*prev)[i][0], (*prev)[i][1], (*prev)[i][2], (*prev)[i][3]);
			net[i][prev_index] = net[i][prev_index] + alpha * ( 0 - prev_value);
		}
	}

	unsigned long index_to_index(int index0, int index1, int index2, int index3){
		return index0*4096 + index1 * 256 + index2 * 16 + index3;
	}

	action take_action(const board& before){
		float max_gain = 0;
		int max_move = -1;

		for(int i = 0; i < 4; i++){
			float current_value = 0;
			for(int j = 0; j < 4; j++) current_value += net[j][index_to_index((*current)[0][j], (*current)[1][j], (*current)[2][j], (*current)[3][j])];
			for(int j = 0; j < 4; j++) current_value += net[i][index_to_index((*current)[j][0], (*current)[j][1], (*current)[j][2], (*current)[j][3])];
			board::reward reward = board(before).slide(i);
			if(reward == -1) continue;
			else{
				if(current_value + reward > max_gain){
					max_gain = current_value + reward;
					max_move = i;
				}
			}
		}


		return max_move == -1 ? action() : action::slide(max_move);
	}

private:
	board *prev, *current;
};