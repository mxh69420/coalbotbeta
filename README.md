# coalbotbeta
working on a c++ discord library called coal, i kinda spun up an implementation so i could learn how the api works. do what you want with it i guess

# warning:
this implementation is experimental. i am not responsible for any damages, from my rate limit manager falling apart and causing your keys to get revoked to some buffer overflow thats so bad it causes your computer to undergo nuclear fission and destroy the city.

speaking of which, i do still get 429s, meaning your token has a risk of being banned. dont use this with any bot that has a presence in servers, and definitely dont use your actual user account. probably dont use this at all, just read the code (though its kinda unreadable) and do it right.

# compiling:
do `make -j3`. there are only 3 .cpp files (4 if you count one that i ended up having included instead)
  
# running:
AUTH_TOK="(your token)" ./a.out
