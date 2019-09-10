#include <iostream>
#include <string>
#include <random>
#include <sys/random.h>
#include <ctype.h>

#define DUMP(x) (std::cout << #x ": " << x << '\n')

unsigned long kb_rand(double lo, double hi){
	hi -= lo;
	double i_max = pow(hi/2.0, 1.0/3.0);
	std::uniform_real_distribution<double> unif(i_max * -1, i_max);
//	DUMP(i_max);

	static std::default_random_engine re;
	static bool seeded = false;

	if(!seeded){
		std::default_random_engine::result_type seed;
		getrandom(&seed, sizeof(seed), 0);
		re.seed(seed);
		seeded = true;
	}

	double r_val = unif(re);
//	DUMP(r_val);

	double ret = pow(r_val, 3.0);
//	DUMP(ret);

	ret += hi/2.0;
	ret += lo;
//	std::cout << "kb_time: returning " << ret << " seconds" << std::endl;
	return ret * 1000;
}

bool donut_eater(const std::string_view &str){
	std::string lstr;
	lstr.reserve(str.length());
	for(auto it = str.begin(); it != str.end(); ++it){
		if(isupper(*it)) lstr.push_back(*it - 'A' + 'a');
		else if(islower(*it)) lstr.push_back(*it);
		else if(
			(isspace(*it) || *it == '-' || *it == '_') &&
			lstr.back() != '-'
		) lstr.push_back('-');
	}
	return lstr == "do-you-know-who-ate-all-the-donuts";
}
