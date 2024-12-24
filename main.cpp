#include <array>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <cmath>
#include <fstream>
#include <iostream>

//fix: fisheye, collision when reversing, button toggles, level editor
//add: textures, up/down, gameplay, gamify, tall/short

int main() {
    //settings
    constexpr int screenW=900; //in px
    constexpr int screenH=600;
    constexpr int row=20; //number of rows in map
    constexpr int column=20; //number of columns in map
    constexpr int shadowStrength=15;
    constexpr int speed=100; //player movement speed
    constexpr int res=200; //resolution
    constexpr float coefficient=0.3;
    constexpr int fov=coefficient*res; //field of view in degrees
    constexpr float wallHeight1=2; //heights of respective wall types
    constexpr float wallHeight2=1;
    constexpr float wallHeight3=1;
    constexpr float tol=10; //px from wall where collision activates

    //colours
    sf::Color grey(180,180,180);
    sf::Color lightGrey(240,240,240);
    sf::Color darkGrey(140,140,140);
    sf::Color red(255,0,0); //1
    sf::Color green(0,255,0); //2
    sf::Color blue(0,0,255); //3
    sf::Color generic(255,255,255);

    //map
    int map[row][column];
    int rowCounter=0;
    int columnCounter=0;
    std::ifstream mapFileIn("map.txt");
    if(!mapFileIn){throw std::runtime_error("error: open file for input failed!\n");}
    while (!mapFileIn.eof()) {
        int i;
        mapFileIn>>i;
        map[rowCounter][columnCounter]=i;
        columnCounter++;
        if (columnCounter%column==0){columnCounter=0;rowCounter++;}
    }
    mapFileIn.close();

    //grid
    int numPoints=2*row+2*column+2;
    sf::VertexArray grid(sf::Lines, numPoints);
    float gridX=0;
    float gridY=0;
    for (int i=0;i<2*column+2;i++) { //ver
        grid[i].position=sf::Vector2f(gridX,0);
        i++;
        grid[i].position=sf::Vector2f(gridX,screenH);
        gridX+=screenW/column;
    }
    for (int i=2*column+2;i<numPoints;i++) { //hor
        grid[i].position=sf::Vector2f(0,gridY);
        i++;
        grid[i].position=sf::Vector2f(screenW,gridY);
        gridY+=screenH/row;
    }
    sf::RectangleShape tile;
    tile.setSize(sf::Vector2f(screenW/column, screenH/row));
    tile.setFillColor(grey);

    //ceiling and floor
    sf::RectangleShape ceiling;
    ceiling.setSize(sf::Vector2f(screenW,screenH/2));
    ceiling.setFillColor(lightGrey);
    sf::RectangleShape floor;
    floor.setSize(sf::Vector2f(screenW,screenH/2));
    floor.setPosition(sf::Vector2f(0,screenH/2));
    floor.setFillColor(darkGrey);

    //player
    sf::Vector3f p(screenW/2,screenH/2,0); //position and direction (x, y, theta in rad)
    sf::ConvexShape player;
    player.setPointCount(3);
    player.setPoint(0, sf::Vector2f(p.x,p.y)); //drawing as triangle
    player.setPoint(1, sf::Vector2f(p.x-20,p.y-5.36));
    player.setPoint(2, sf::Vector2f(p.x-20,p.y+5.36));
    player.setFillColor(sf::Color::Green);
    player.setOrigin(p.x,p.y);
    int setSpeed; //used for logic

    //ray
    std::array<sf::VertexArray, res> ray;
    for (int i=0;i<res;i++) {
        ray[i]=sf::VertexArray(sf::PrimitiveType::Lines,2);
        ray[i][0].color=sf::Color::Red;
        ray[i][1].color=sf::Color::Red;
    }
    std::array<float, res> rl;

    //algorithm use
    std::array<int, res> side; //used for logic and to select wall colour (1 is ver, 0 is hor)
    sf::Vector2f scaledPos; //scaled position
    sf::Vector2i truncatedPos; //truncated scaled position
    sf::Vector2f internalPos; //internal position in square
    sf::Vector2f n; //normalised direction vector (max length 1)
    sf::Vector2f distX; //x: dist to first ver intercept, y: dist between consecutive ver intercepts
    sf::Vector2f distY; //x: dist to first hor intercept, y: dist between consecutive hor intercepts
    sf::Vector2f unit; //x: 1 or -1 step for ver intercept, y: 1 or -1 step for hor intercept
    float wallDist; //dist to wall from camera plane

    //misc
    sf::RenderWindow window(sf::VideoMode(screenW, screenH), "Ray-Caster", sf::Style::Titlebar | sf::Style::Close);
    sf::Clock clock;
    bool menu=false;
    std::array<sf::RectangleShape, res> wall;

    while (window.isOpen()) {

        //fps and time setup
        window.setFramerateLimit(60);
        sf::Time dt=clock.restart(); //delta time

        //keyboard commands
        sf::Event event;
        while (window.pollEvent(event)) {
            //close
            if (event.type==sf::Event::Closed) window.close();
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::Escape) window.close();

            //menu
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::E) menu=true;
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::E&&
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) menu=false;

            //level editing
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                event.type==sf::Event::KeyPressed&&
                event.key.code==sf::Keyboard::Num1&&
                menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=1;
            }
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                event.type==sf::Event::KeyPressed&&
                event.key.code==sf::Keyboard::Num2&&
                menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=2;
            }
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                event.type==sf::Event::KeyPressed&&
                event.key.code==sf::Keyboard::Num3&&
                menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=3;
            }
            if(sf::Mouse::isButtonPressed(sf::Mouse::Left)&&
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)&&menu==true) {
                map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=0;
            }
        }

        //keyboard control setup
        p.z=player.getRotation()*M_PI/180;
        setSpeed=speed;

        //DDA algo. checks grid+1 in direction of next nearest intercept for wall
        for (int i=0;i<res;i++) {
            n.x=cos(p.z+(-(fov/2)+i*coefficient)*M_PI/180);
            n.y=sin(p.z+(-(fov/2)+i*coefficient)*M_PI/180);
            if (n.y==0)n.y=0.00001; //avoiding division by zero

            scaledPos={column*p.x/screenW,row*p.y/screenH};
            truncatedPos={static_cast<int>(scaledPos.x),static_cast<int>(scaledPos.y)};
            internalPos={scaledPos.x-static_cast<float>(truncatedPos.x),
            scaledPos.y-static_cast<float>(truncatedPos.y)};

            if (n.x>0) {
                distX.x=(1-internalPos.x)/n.x;
                unit.x=1;
                distX.y=1/n.x;
            } else {
                distX.x=internalPos.x/n.x;
                unit.x=-1;
                distX.y=1/n.x;
            }
            if (n.y>0) {
                distY.x=(1-internalPos.y)/n.y;
                unit.y=1;
                distY.y=1/n.y;
            } else {
                distY.x=internalPos.y/n.y;
                unit.y=-1;
                distY.y=1/n.y;
            }

            while (true) {
                if (abs(distX.x)<abs(distY.x)) { //true if distance to next ver intercept shorter
                    side[i]=1;
                    distX.x+=distX.y;
                    truncatedPos.x+=unit.x;
                } else {
                    side[i]=0;
                    distY.x+=distY.y;
                    truncatedPos.y+=unit.y;
                }
                if (map[truncatedPos.y][truncatedPos.x]!=0)break;
            }

            //ray calculation
            ray[i][0].position={p.x,p.y};
            if (side[i]==1) {
                ray[i][1].position={p.x+n.x*screenW*abs(distX.x-distX.y)/column,
                p.y+n.y*screenH*abs(distX.x-distX.y)/row};
            } else ray[i][1].position={p.x+n.x*screenW*abs(distY.x-distY.y)/column,
                p.y+n.y*screenH*abs(distY.x-distY.y)/row};
            ray[i][0].position={p.x,p.y};
            rl[i]=sqrt(pow((ray[i][1].position.x-ray[i][0].position.x),2)
            +pow((ray[i][1].position.y-ray[i][0].position.y),2));

            //collision detection
            if (i==fov&&rl[i]<tol)setSpeed=0;

            //wall setup
            side[i]==1 ? wallDist=(abs(distX.x)-abs(distX.y)) : wallDist=(abs(distY.x)-abs(distY.y));
            switch (map[truncatedPos.y][truncatedPos.x]) {
                case 1:
                    generic=red;
                    if (side[i]==1)generic.r-=(2*shadowStrength);
                    wall[i].setSize(sf::Vector2f(1+screenW/res, wallHeight1*screenH/wallDist));
                    wall[i].setOrigin(screenW/column,wall[i].getSize().y);
                    wall[i].setPosition((screenW/column+i*screenW/res),screenH/2+screenH/(2*wallDist));
                    break;
                case 2:
                    generic=green;
                    if (side[i]==1)generic.g-=(2*shadowStrength);
                    wall[i].setSize(sf::Vector2f(1+screenW/res, screenH/wallDist));
                    wall[i].setOrigin(screenW/column,wall[i].getSize().y/2);
                    wall[i].setPosition((screenW/column+i*screenW/res),(screenH/2));
                    break;
                case 3:
                    generic=blue;
                    if (side[i]==1)generic.b-=(2*shadowStrength);
                    wall[i].setSize(sf::Vector2f(1+screenW/res, screenH/wallDist));
                    wall[i].setOrigin(screenW/column,wall[i].getSize().y/2);
                    wall[i].setPosition((screenW/column+i*screenW/res),(screenH/2));
                    break;
                default:
                    generic=green;
                    if (side[i]==1)generic.g-=(2*shadowStrength);
                    wall[i].setSize(sf::Vector2f(1+screenW/res, screenH/wallDist));
                    wall[i].setOrigin(screenW/column,wall[i].getSize().y/2);
                    wall[i].setPosition((screenW/column+i*screenW/res),(screenH/2));
                    break;
            }
            generic.r-=rl[i]*shadowStrength/res;
            generic.g-=rl[i]*shadowStrength/res;
            generic.b-=rl[i]*shadowStrength/res;
            wall[i].setFillColor(generic);
            generic.r=255;generic.g=255;generic.b=255;
        }

        //keyboard control
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) { //forwards
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) {setSpeed*=3;} //zoomies
            p.x+=cos(p.z)*setSpeed*dt.asSeconds(); p.y+=sin(p.z)*setSpeed*dt.asSeconds();
        } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) { //backwards
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) {setSpeed*=3;} //zoomies
            p.x-=cos(p.z)*setSpeed*dt.asSeconds(); p.y-=sin(p.z)*setSpeed*dt.asSeconds();
        }
        player.setPosition(p.x, p.y);
        setSpeed=speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) { //rotate ccw
            player.setRotation(player.getRotation()-1.8*setSpeed*dt.asSeconds());
        } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) { //rotate cw
            player.setRotation(player.getRotation()+1.8*setSpeed*dt.asSeconds());}

        if (menu==true) {
            window.clear(sf::Color::Black);
            for(int i=0;i<row;i++) {
                for(int j=0;j<column;j++){
                    if(map[i][j]!=0)
                    {
                        tile.setPosition(j*screenW/column,i*screenH/row);
                        window.draw(tile);
                    }
                }
            }
            window.draw(grid);
            window.draw(player);
            for (int i=0;i<res;i++) {
                window.draw(ray[i]);
            }
            window.display();
        } else {
            //rendering
            window.clear(sf::Color::Black);
            window.draw(ceiling);
            window.draw(floor);
            for (int i=0;i<res;i++) {
                window.draw(wall[i]);
            }
            window.display();
            //std::cout<<(1/dt.asSeconds())<<'\n'; //fps
        }
    }
    //saving map
    std::ofstream mapFileOut("map.txt");
    if(!mapFileOut){throw std::runtime_error("error: open file for output failed!\n");}
    for(int i=0;i<row;i++) {
        for(int j=0;j<column;j++){
            mapFileOut<<map[i][j]<<'\n';
        }
    }
    mapFileOut.close();
    return 0;
}

