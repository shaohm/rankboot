#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>

#define BUFFER_SIZE (1024LL * 1024LL * 1024LL * 1LL)
#define BUFFER_SIZE (1024LL * 1024LL)

using namespace std;

class Example { // implys g(first) < g(second)
public:
	unsigned char *first;
	unsigned char *second;
	double weight;
	double score_first, score_second;
};

class Session {
public:
	char query_id[64];
	vector<unsigned char *> firsts; 
	vector<unsigned char *> seconds;
	vector<Example> examples;
	int right, wrong, equal;
	Session() {
		query_id[0] = '\0';
	}
private:
	Session(const Session &o) {
	}
};

class Dataset {
public:
	// feature statistics
	int num_features;
	map<int,int> F; // Forward: outFeatureNo -> inFeatureNo
	map<int,int> B; // Backword: inFeatureNo -> outFeatureNo
	unsigned char *min;
	unsigned char *max;
	
	// data structures
	vector<Session *> sessions; 
	unsigned char *buffer;
	Dataset(){};
private:
	Dataset(const Dataset &o) {}
};
ostream& operator <<(ostream& out, Dataset &ds) {
	out << "num_features=" << ds.num_features << endl;
	out << "F=";
	for(map<int,int>::iterator iter = ds.F.begin(), end = ds.F.end(); iter != end; iter++) 
		out << (*iter).first << ":" << (*iter).second << " ";
	out << endl;
	
	out << "B=";
	for(map<int,int>::iterator iter = ds.B.begin(), end = ds.B.end(); iter != end; iter++) 
		out << (*iter).first << ":" << (*iter).second << " ";
	out << endl;
	
	out << "min=[";
	for(int f = 0; f < ds.num_features; f++)
		out << (int)ds.min[f] << ", ";
	out << "]" << endl;
	out << "max=[";
	for(int f = 0; f < ds.num_features; f++)
		out << (int)ds.max[f] << ", ";
	out << "]" << endl;
	out << "sessions=" << endl;
	for(int i = 0; i < ds.sessions.size(); i ++) {
		out << "    " << i << ".  " << (ds.sessions[i]->query_id);
		out << "  nfirsts=" << ds.sessions[i]->firsts.size();
		out << "  nseconds=" << ds.sessions[i]->seconds.size();
		out << "  nexamples=" << ds.sessions[i]->examples.size();
		out << "  weight_sample=" << ds.sessions[i]->examples[0].weight;
		out << endl;
		out << "        " << "[" << (int)ds.sessions[i]->examples[0].first[0] << ", " << (int)ds.sessions[i]->examples[0].first[1] << "]" << endl;
		out << "        " << "[" << (int)ds.sessions[i]->examples[0].second[0] << ", " << (int)ds.sessions[i]->examples[0].second[1] << "]" << endl;
	}
	return out;
};


void loadDataset(Dataset *ds, const char* datfn, const char* mapfn) {
	// read feature description
	ds->num_features = 0;
	FILE *mf = fopen(mapfn, "r");
	char linebuf[1024];
	while(fgets(linebuf, 1024, mf)) {
		int feature_no = atoi(linebuf);
		ds->F[feature_no] = ds->num_features;
		ds->B[ds->num_features] = feature_no;
		ds->num_features += 1;
	}
 	ds->min = new unsigned char[ds->num_features]();
 	ds->max = new unsigned char[ds->num_features]();
 	for(int f = 0; f < ds->num_features; f++) {
	 	ds->min[f] = 255;
	 	ds->max[f] = 0;
	}
	fclose(mf);
	//debug
	cout << "feature map loaded" << endl;
	
	// read data
	ds->buffer = new unsigned char[BUFFER_SIZE]();
	FILE *df = fopen(datfn, "r");
	long long int instance_no = 0;
	Session *session;
	while(fgets(linebuf, 1024, mf)) {
		//debug
		cout << "instance: " << instance_no << endl;
		char *label_str = strtok(linebuf, " \t");
		int label = strtol(label_str, NULL, 10);
		char qid[64]; 
		char *qidcp = strtok(NULL, " \t");
		strcpy(qid, qidcp);
		
		long long int offset = instance_no * ds->num_features;
		while(true) {
			char *feature_no_str = strtok(NULL, ": \t");
			if(feature_no_str == NULL)
				break;
			char *endptr;
			int feature_no = strtol(feature_no_str, &endptr, 10);
			if(feature_no_str == endptr) {
				break;
			}
			feature_no = ds->F[feature_no];
			char *value_str = strtok(NULL, " \t");
			int value = strtol(value_str, NULL, 10);
			assert(value >= 0 && value < 256);
			ds->buffer[offset + feature_no] = value;
		}
		//compute feature statistics
		for(int f = 0; f < ds->num_features; f++) {
			if(ds->buffer[offset + f] < ds->min[f])
				ds->min[f] = ds->buffer[offset + f];
			if(ds->buffer[offset + f] > ds->max[f])
				ds->max[f] = ds->buffer[offset + f];
		}
		
		if(instance_no == 0 || strcmp(qid, session->query_id) != 0) {
			//finish curr session
			if(instance_no > 0) {
				for(int i = 0; i < session->firsts.size(); i++) {
					unsigned char *first = session->firsts[i];
					for(int j = 0; j < session->seconds.size(); j++) {
						unsigned char *second = session->seconds[j];
						Example e;
						e.first = first;
						e.second = second;
						e.weight = 1.0 / (session->firsts.size() * session->seconds.size());
						e.score_first = e.score_second = .0;
						session->examples.push_back(e);
					}
				}
				session->right = session->wrong = session->equal = 0;
				if(session->examples.size() > 0)
					ds->sessions.push_back(session);
			}
			session = new Session(); // create new session
			strcpy(session->query_id, qid);
		}
		if(label == 0)
			session->firsts.push_back(ds->buffer + offset);
		else
			session->seconds.push_back(ds->buffer + offset);
		instance_no += 1;
	}
	//finish last session
	for(int i = 0; i < session->firsts.size(); i++) {
		unsigned char *first = session->firsts[i];
		for(int j = 0; j < session->seconds.size(); j++) {
			unsigned char *second = session->seconds[j];
			Example e;
			e.first = first;
			e.second = second;
			e.weight = 1.0 / (session->firsts.size() * session->seconds.size());
			e.score_first = e.score_second = .0;
			session->examples.push_back(e);
		}
	}
	session->right = session->wrong = session->equal = 0;
	if(session->examples.size() > 0)
		ds->sessions.push_back(session);
	
};


class WeakLearner {
public:
	int feature; // which feature
	unsigned char theta; // split
	double alpha; // weight of this weak learner
	double w_right, w_wrong, w_equal;
	void learn(Dataset &ds);
};
void print_weak_ranker(ostream &out, WeakLearner &wl, Dataset* ds) {
	out << ds->B[wl.feature] << "\t";
	out << (int)wl.theta << "\t" ;
	out << wl.alpha << "\t#\t";
	out << wl.w_right << "\t";
	out << wl.w_wrong << "\t" ;
	out << wl.w_equal;
};

void WeakLearner::learn(Dataset &ds) {
	double right[ds.num_features][255];
	double wrong[ds.num_features][255];
	double equal[ds.num_features][255];
	for(int f = 0; f < ds.num_features; f++) {
		for(int v = 0; v < 256; v++) {
			right[f][v] = wrong[f][v] = equal[f][v] = 0;
		}
	}
	for(int k = 0; k < ds.sessions.size(); k++) {
		Session *session = ds.sessions[k];
		for(int i = 0; i < session->examples.size(); i++) {
			Example &example = session->examples[i];
			double weight = example.weight;
			unsigned char* first = example.first;
			unsigned char* second = example.second;
			for(int f = 0; f < ds.num_features; f++) {
				if(first[f] > second[f]) {
					for(int v = ds.min[f]; v < second[f]; v++)
						equal[f][v] += weight;
					for(int v = second[f]; v < first[f]; v++)
						wrong[f][v] += weight;
					for(int v = first[f]; v < ds.max[f]; v++)
						equal[f][v] += weight;
				} else if(first[f] < second[f]) {
					for(int v = ds.min[f]; v < first[f]; v++)
						equal[f][v] += weight;
					for(int v = first[f]; v < second[f]; v++)
						right[f][v] += weight;
					for(int v = second[f]; v < ds.max[f]; v++)
						equal[f][v] += weight;
				} else {
					for(int v = ds.min[f]; v < ds.max[f]; v++)
						equal[f][v] += weight;
				}
			}
		}
	}

	// debug
	cout << "right wrong computed" << endl;
	
	int bestFeature;
	unsigned char bestTheta;
	double bestZ = ds.sessions.size() * 2;
	for(int f = 0; f < ds.num_features; f++) {
		for(int v = ds.min[f]; v < ds.max[f]; v++){
			double z = equal[f][v] + 2 * sqrt(right[f][v] * wrong[f][v]);
			if(z < bestZ) {
				bestZ = z;
				bestFeature = f;
				bestTheta = v;
			}
			cout << "feature_score: " << ds.B[f] << " " << v << " " << z << " " << right[f][v] << " " << wrong[f][v] << " " << equal[f][v] << endl;
		}
	}
	// debug
	cout << "best split found" << endl;

	this->feature = bestFeature;
	this->theta = bestTheta;
	this->w_right = right[this->feature][this->theta];
	this->w_wrong = wrong[this->feature][this->theta];
	this->w_equal = equal[this->feature][this->theta];
	this->alpha = 0.5 * log((1e-8 + this->w_right) / (1e-8 + this->w_wrong));
};


int main(int argc, char *argv[])
{
	// debug
	if(argc != 4) {
		cerr << "./rankboost <experiment id> <num weak rankers> <top_n>" << endl;
		return 1;
	}
	char *train_file_name = argv[1];
	int num_weak_rankers = atoi(argv[2]);
	int top_n = atoi(argv[3]);

	// load dataset
	Dataset *ds = new Dataset();
	char datfn[128] = ""; 
	strcat(datfn, train_file_name);
	strcat(datfn, ".dat");
	char mapfn[128] = ""; 
	strcat(mapfn, train_file_name);
	strcat(mapfn, ".map");	
	loadDataset(ds, datfn, mapfn);
	
	char modfn[128] = ""; 
	strcat(modfn, train_file_name);
	strcat(modfn, ".model");
	std::ofstream mout(modfn);
	mout << setprecision(6);

	// debug
	cout << "dataset loaded" << endl;
	
	// initial distribution
	for(int k = 0; k < ds->sessions.size(); k++) {
		Session *session = ds->sessions[k];
		for(int i = 0; i < session->examples.size(); i++) {
			session->examples[i].weight = 1.0 / session->examples.size(); 
		}
	}
	
	WeakLearner rankers[num_weak_rankers];
	for(int i =  0; i < num_weak_rankers; i++) {
		// debug
		cout << i << "th started" << endl;

		// learn a weak learner
		rankers[i].learn(*ds);
		WeakLearner &h = rankers[i];
		
		// debug
		cout << i << "th learned" << endl;
		
		// reweight
		double z = .0;
		for(int k = 0; k < ds->sessions.size(); k++) {
			Session *session = ds->sessions[k];
			session->right = session->wrong = session->equal = 0;
			for(int i = 0; i < session->examples.size(); i++) {
				Example &example = session->examples[i];
				double h_first = .0, h_second = .0;
				if(example.first[h.feature] > h.theta) {
					h_first = 1.0;
					example.score_first += h.alpha;
				}					
				if(example.second[h.feature] > h.theta) {
					h_second = 1.0;
					example.score_second += h.alpha;
				}
				if(example.score_first < example.score_second) {
					session->right += 1;
				} else if(example.score_first > example.score_second) {
					session->wrong += 1;
				} else {
					session->equal += 1;
				}
				
				example.weight *= exp(h.alpha * (h_first - h_second));
				z += example.weight;
			}
		}
		// debug
		cout << i << "th reweighted" << endl;
		// normalize. sum to session numbers
		for(int k = 0; k < ds->sessions.size(); k++) {
			Session *session = ds->sessions[k];
			for(int i = 0; i < session->examples.size(); i++) {
				Example &example = session->examples[i];
				example.weight /= z / ds->sessions.size(); 
			}
		}
		
		// debug
		cout << i << "th normalized" << endl;

		// output new ranker
		double top_n_coverage = .0;
		for(int k = 0; k < ds->sessions.size(); k++) {
			Session *session = ds->sessions[k];
			if(session->wrong + session->equal < top_n) {
				top_n_coverage += 1.0 / ds->sessions.size();
			} else if(session->wrong < top_n) {
				top_n_coverage += (top_n - session->wrong) * 1.0 / (session->equal+1) / ds->sessions.size();
			}
		}
		print_weak_ranker(mout, h, ds);
		mout << "\t" << top_n_coverage << endl;
	}
	mout.close();
	return 0;
}
