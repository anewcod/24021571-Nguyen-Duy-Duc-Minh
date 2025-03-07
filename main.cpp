//bug:
//feature: timer, boss, endless, lore(YAYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY),
//        sword durability (?),stamina,kill count, ...

#include <iostream>
#include <cstdlib>
using namespace std;

int spawn();
void play(int x,int mons, int lives);
void act(int mons, int lives, int mons_lives);
int end();

int base_lives = 1;
bool state;

int spawn(){
    srand(time(0));
    int x = rand()%5;
    return x;
}

void play(bool x,int mons, int lives){
    if(x){
        cout<<"Incoming ";
        int mons_lives;
        switch(mons){
            case 0:cout<<"arrow";mons_lives=0;break;
            case 1:cout<<"foot soldier";mons_lives=1;break;
            case 2:cout<<"spear man";mons_lives=1;break;
            case 3:cout<<"heavy armor";mons_lives=2;break;
            case 4:cout<<"lancer";mons_lives=2;break;
        }
        cout<<".\n";
        act(mons,lives,mons_lives);
    }
    else cout<<"Thanks for playing.";
}

void act(int mons, int lives, int mons_lives){
    int n,hits=0;
    cout<<"0/attack         1/block\n";
    cin>>n;
    switch(mons){
        case 0:if(n!=1) hits++;
        default: if(n==0) mons_lives--; 
    }
    if(hits>0){
        lives--;
        if(lives>1) {
            cout<<"Becareful, you only have "<<lives<<" lives left.\n";
            play(state,spawn(),lives);
            }
        else if(lives == 1) {
            cout<<"Last live brother. Don't die.\n";
            play(state,spawn(),lives);
            }
        else cout<<"Well, you die. The enemies have enter the castle. May the king be safe.\n"
                 <<"But what if you could do it again, will you\n"
                 <<"0/New game                  1/That's the end for me\n";
                 cin>>n;
                 if(n==0){
                     cout<<"Very well, let's try it again";
                     play(state,spawn(),base_lives);
                 }
                 else {
                     cout<<"Got it, I will meet you soon, in Vahalla.";
                     state = false;
                 }
    }
    if(mons_lives>0){
        cout<<"The enemy is still alive.\n";
        act(mons, lives, mons_lives);
    }
    else play(state,spawn(),lives);
}

int main()
{
    int x;
    cout<<"So you're a knight, defend the castle by all cost. Wanna try, 1 to play\n";
    cin>>x;
    if(x == 1){
        state = true;
        cout<<"Ok, so 0 to attack, 1 to block, got it. Now go!!";
        play(state,spawn(),base_lives);
    }
}
