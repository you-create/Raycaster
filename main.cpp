#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <array>
#include <stack>
#include <queue>
#include <cmath>
#include <fstream>
#include <iostream>


//fix: mild fisheye, collision, button toggles, level editor, block top, alias adjust, inner wall textures, floor, doors+portals
//add: y shearing, gameplay, sky, jump, angles (intersection method?), mouse control, more textures (plasma?)

int main() {
    //settings
    constexpr bool testFPS=0; //if true prints max fps
    constexpr int screenW=900; //in px
    constexpr int screenH=600;
    constexpr int row=20; //number of rows in map
    constexpr int column=20; //number of columns in map
    constexpr int shadowStrength=50;
    constexpr int speed=100; //player movement speed
    constexpr int res=200; //resolution
    constexpr float coefficient=0.3;
    constexpr int fov=coefficient*res; //field of view in degrees
    constexpr float wallHeight1=2; //heights of respective wall types
    constexpr float wallHeight2=2;
    constexpr float wallHeight3=1;
    constexpr float wallHeight4=1;
    constexpr float wallHeight5=2;
    constexpr float wallHeight6=2;
    constexpr float aliasAdjust=0.3; //changes width of tex rects to reduce aliasing effects

    //colours
    sf::Color white(255,255,255);
    sf::Color darkGrey(140,140,140);
    sf::Color darkestGrey(80,80,80);

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

    //ceiling and floor
    sf::RectangleShape ceiling;
    ceiling.setSize(sf::Vector2f(screenW,screenH/2));
    ceiling.setFillColor(darkGrey);
    sf::RectangleShape floor;
    floor.setSize(sf::Vector2f(screenW,screenH/2));
    floor.setPosition(sf::Vector2f(0,screenH/2));
    floor.setFillColor(darkestGrey);

    //textures
    sf::Texture largePlainBricksTex;
    largePlainBricksTex.loadFromFile("large_plain_bricks.png");
    sf::Texture brickTowerTex;
    brickTowerTex.loadFromFile("brick_tower.png");
    sf::Texture bannerBlueTex;
    bannerBlueTex.loadFromFile("banner_blue.png");
    sf::Texture bannerRedTex;
    bannerRedTex.loadFromFile("banner_red.png");
    sf::Texture door4Tex;
    door4Tex.loadFromFile("short_door.png");
    sf::Texture portalTex;
    portalTex.loadFromFile("orange_xor_256x512.png");
    sf::Texture emptyTex;
    emptyTex.loadFromFile("empty.png");
    sf::Texture knife2Tex;
    knife2Tex.loadFromFile("knife2.png");

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

    //npc
    sf::Vector3f e(screenW/2,screenH/2,0); //position and direction (x, y, theta in rad)
    sf::ConvexShape enemy;
    enemy.setPointCount(3);
    enemy.setPoint(0, sf::Vector2f(p.x,p.y)); //drawing as triangle
    enemy.setPoint(1, sf::Vector2f(p.x-20,p.y-5.36));
    enemy.setPoint(2, sf::Vector2f(p.x-20,p.y+5.36));
    enemy.setFillColor(sf::Color::Red);
    enemy.setOrigin(p.x,p.y);
    enemy.setPosition(screenW/2,screenH/2);

    //canvas
    sf::Sprite knifeSpr;
    knifeSpr.setTexture(knife2Tex);
    knifeSpr.setPosition((screenW-320),(screenH-320));
    knifeSpr.setScale(10,10);
    sf::CircleShape crosshair;
    crosshair.setPosition({(static_cast<float>(screenW)/2.f),(static_cast<float>(screenH)/2.f)});
    crosshair.setRadius(2);

    //text
    sf::Font font;
    font.loadFromFile("ARIAL.TTF");
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(24);
    text.setFillColor(sf::Color::White);
    text.setPosition(10,10);

    //algorithm use
    int side;
    sf::Vector2f scaledPos; //scaled position
    sf::Vector2i truncatedPos; //truncated scaled position
    sf::Vector2f internalPos; //internal position in square
    sf::Vector2f n; //normalised direction vector (max length 1)
    sf::Vector2f distX; //x: dist to first ver intercept, y: dist between consecutive ver intercepts
    sf::Vector2f distY; //x: dist to first hor intercept, y: dist between consecutive hor intercepts
    sf::Vector2f unit; //x: 1 or -1 step for ver intercept, y: 1 or -1 step for hor intercept
    float wallDist; //dist to wall from camera plane
    float interceptXPos; //pos of hor intercept along wall (0~1). used for texture
    float interceptYPos; //pos of ver intercept along wall (0~1). used for texture
    struct RayInfo { //info on each hit of each ray (multiple for dif square types)
        int id; //hit type
        int side;
        float distX; //distX to hit
        float distY; //distY to hit
        float interceptXPos; //hor position of hit 0~1 on face
        float interceptYPos; //ver position of hit 0~1 on face
    };
    std::stack<RayInfo> renderStack; //remembers order of hits to then render in reverse

    //misc
    sf::RenderWindow window(sf::VideoMode(screenW, screenH), "Ray-Caster", sf::Style::Titlebar | sf::Style::Close);
    bool lastState=false; //for menu
    bool curState=false;
    sf::Clock clock;
    sf::Clock clockTotal;
    sf::Clock clockFrame;
    sf::Time timeFrame;
    bool menu=false;
    std::queue<sf::RectangleShape> wall;
    float wallHeight;
    int frameCounter=0;
    int fps;

    while (window.isOpen()) {

        //general setup
        if (testFPS==false)window.setFramerateLimit(60);
        sf::Time dt=clock.restart(); //delta time
        sf::Time timeTotal=clockTotal.getElapsedTime();

        //keyboard commands
        sf::Event event;
        while (window.pollEvent(event)) {
            //close
            if (event.type==sf::Event::Closed) window.close();
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::Escape) window.close();

            //menu
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::E){lastState=curState;curState=true;}
            else{curState=false;}
            if (lastState!=curState)menu==true?menu=false:menu=true;

            //level editing
            if(menu==true) {
                switch (event.key.code) {
                    case sf::Keyboard::Num0://delete block
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=0;
                    break;
                    case sf::Keyboard::Num1://red banner tower
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=1;
                    break;
                    case sf::Keyboard::Num2://blue banner tower
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=2;
                    break;
                    case sf::Keyboard::Num3://internal plain block
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=3;
                    break;
                    case sf::Keyboard::Num4://edge plain block (end condition)
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=4;
                    break;
                    case sf::Keyboard::Num5://door (end condition)
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=5;
                    break;
                    case sf::Keyboard::Num6://portal
                        map[static_cast<int>(row*sf::Mouse::getPosition(window).y/screenH)]
                        [static_cast<int>(column*sf::Mouse::getPosition(window).x/screenW)]=6;
                    break;
                    default:
                    break;
                }
            }
        }

        //general setup
        p.z=player.getRotation()*M_PI/180; //[0,2*M_PI)
        setSpeed=speed;

        //hor portal
        scaledPos={column*p.x/screenW,row*p.y/screenH};
        truncatedPos={static_cast<int>(scaledPos.x),static_cast<int>(scaledPos.y)};
        if(map[truncatedPos.x][truncatedPos.y]==4) {
            p.x=screenW-p.x;
            player.setPosition(p.x, p.y);
        }

        //DDA algo. checks grid+1 in direction of next nearest intercept for wall
        for (int i=0;i<res;i++) {
            scaledPos={column*p.x/screenW,row*p.y/screenH};
            truncatedPos={static_cast<int>(scaledPos.x),static_cast<int>(scaledPos.y)};
            internalPos={scaledPos.x-static_cast<float>(truncatedPos.x),
            scaledPos.y-static_cast<float>(truncatedPos.y)};

            n.x=cos(p.z+(-(fov/2)+i*coefficient)*M_PI/180);
            n.y=sin(p.z+(-(fov/2)+i*coefficient)*M_PI/180);
            if (n.y==0)n.y=0.00001; //avoiding division by zero

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

            //ray casting loop
            while (true) {
                if (abs(distX.x)<abs(distY.x)) { //true if distance to next ver intercept shorter
                    side=1;
                    distX.x+=distX.y;
                    truncatedPos.x+=unit.x;
                } else {
                    side=0;
                    distY.x+=distY.y;
                    truncatedPos.y+=unit.y;
                }

                //finding position of intercept along wall (0~1) for texture
                interceptXPos=scaledPos.x+(abs(distY.x)-abs(distY.y))*n.x;
                interceptYPos=scaledPos.y+(abs(distX.x)-abs(distX.y))*n.y;
                if (interceptXPos>1)interceptXPos=interceptXPos-static_cast<int>(interceptXPos);
                if (interceptYPos>1)interceptYPos=interceptYPos-static_cast<int>(interceptYPos);
                if (n.x>0&&n.y>0) { //q1
                    interceptXPos=1-interceptXPos;
                } else if (n.x<0&&n.y>0) { //q2
                    interceptXPos=1-interceptXPos;
                    interceptYPos=1-interceptYPos;
                } else if (n.x<0&&n.y<0) { //q3
                    interceptYPos=1-interceptYPos;
                }

                if (map[truncatedPos.y][truncatedPos.x]==4||map[truncatedPos.y][truncatedPos.x]==6) { //end condition
                    renderStack.push({map[truncatedPos.y][truncatedPos.x],side,distX.x,distY.x,interceptXPos,interceptYPos});
                    break;
                }
                if (map[truncatedPos.y][truncatedPos.x]!=0) { //closer intercept
                    renderStack.push({map[truncatedPos.y][truncatedPos.x],side,distX.x,distY.x,interceptXPos,interceptYPos});
                }
            }

            //wall setup
            while (!renderStack.empty()) {
                float angle;
                if (i<(res/2)) {
                    angle=(1.f-(static_cast<float>(i)/(static_cast<float>(res)/2.f)))*static_cast<float>(fov)/2.f;
                } else {
                    angle=((static_cast<float>(i)-(static_cast<float>(res)/2.f))/(static_cast<float>(res)/2.f))
                    *static_cast<float>(fov)/2.f;
                }
                renderStack.top().side==1 ? wallDist=(abs(renderStack.top().distX)-abs(distX.y))*cos(angle*(M_PI/180)) :
                wallDist=(abs(renderStack.top().distY)-abs(distY.y))*cos(angle*(M_PI/180));

                if (renderStack.top().side==1) {
                    white.r-=(0.3*shadowStrength);
                    white.g-=(0.3*shadowStrength);
                    white.b-=(0.3*shadowStrength);
                }
                white.r-=wallDist*wallDist*shadowStrength/res;
                white.g-=wallDist*wallDist*shadowStrength/res;
                white.b-=wallDist*wallDist*shadowStrength/res;
                sf::RectangleShape rect;
                rect.setFillColor(white);
                white.r=255;white.g=255;white.b=255;

                switch (renderStack.top().id) {
                    case 1:
                        wallHeight=wallHeight1;
                        renderStack.top().side==1 ? rect.setTexture(&bannerRedTex) : rect.setTexture(&brickTowerTex);
                        break;
                    case 2:
                        wallHeight=wallHeight2;
                        renderStack.top().side==1 ? rect.setTexture(&bannerBlueTex) : rect.setTexture(&brickTowerTex);
                        break;
                    case 3:
                        wallHeight=wallHeight3;
                        rect.setTexture(&largePlainBricksTex);
                        break;
                    case 4:
                        wallHeight=wallHeight4;
                        rect.setTexture(&largePlainBricksTex);
                    break;
                    case 5:
                        wallHeight=wallHeight5;
                        renderStack.top().side==1 ? rect.setTexture(&door4Tex) : rect.setTexture(&emptyTex);
                    break;
                    case 6:
                        wallHeight=wallHeight6;
                        rect.setTexture(&portalTex);
                    break;
                    default:
                        wallHeight=1;
                        rect.setTexture(&largePlainBricksTex);
                        break;
                }

                rect.setSize(sf::Vector2f(1+screenW/res, wallHeight*screenH/wallDist));
                rect.setOrigin(screenW/column,rect.getSize().y);
                rect.setPosition((screenW/column+i*screenW/res),(screenH/2+screenH/(2*wallDist)));
                if (renderStack.top().id==5&&wallDist<4)rect.setPosition((screenW/column+i*screenW/res),(screenH/2-screenH/(2*wallDist)));
                int texStartCoordX;
                renderStack.top().side==1 ? texStartCoordX=renderStack.top().interceptYPos*32 :
                texStartCoordX=renderStack.top().interceptXPos*32;
                //separating short from long walls. should make constexpr
                if (renderStack.top().id==1||renderStack.top().id==2){rect.setTextureRect(sf::IntRect({texStartCoordX, 0}, {static_cast<int>(wallDist*aliasAdjust), 64}));}
                else {rect.setTextureRect(sf::IntRect({texStartCoordX, 0}, {static_cast<int>(wallDist*aliasAdjust), 32}));}
                //portal animation
                if (renderStack.top().id==6&&(timeTotal.asSeconds()-static_cast<int>(timeTotal.asSeconds()))<0.5){texStartCoordX*=8;rect.setTextureRect(sf::IntRect({texStartCoordX, 0}, {static_cast<int>(wallDist*aliasAdjust), 256}));}
                else if (renderStack.top().id==6&&(timeTotal.asSeconds()-static_cast<int>(timeTotal.asSeconds()))<1){texStartCoordX*=8;rect.setTextureRect(sf::IntRect({texStartCoordX, 256}, {static_cast<int>(wallDist*aliasAdjust), 256}));}
                wall.push(rect);

                renderStack.pop();
            }
        }

        //knife
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) {
            knifeSpr.setTextureRect(sf::IntRect({0,32},{32,32}));
        } else {knifeSpr.setTextureRect(sf::IntRect({0,0},{32,32}));}

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
            player.setRotation(player.getRotation()+1.8*setSpeed*dt.asSeconds());
        }

        //drawing level editor
        if (menu==true) {
            window.clear(sf::Color::Black);
            for(int i=0;i<row;i++) { //drawing tiles with respective textures
                for(int j=0;j<column;j++){
                    switch (map[i][j]) {
                        case 1:
                            tile.setTexture(&bannerRedTex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {32,64}));
                        break;
                        case 2:
                            tile.setTexture(&bannerBlueTex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {32,64}));
                        break;
                        case 3:
                            tile.setTexture(&largePlainBricksTex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {32,32}));
                        break;
                        case 4:
                            tile.setTexture(&largePlainBricksTex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {32,32}));
                        break;
                        case 5:
                            tile.setTexture(&door4Tex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {32,32}));
                        break;
                        case 6:
                            tile.setTexture(&portalTex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {256,256}));
                        break;
                        default:
                            tile.setTexture(&emptyTex);
                            tile.setTextureRect(sf::IntRect({0, 0}, {32,32}));
                        break;
                    }
                        tile.setPosition(j*screenW/column,i*screenH/row);
                        window.draw(tile);
                }
            }
            window.draw(grid);
            window.draw(player);
            window.draw(enemy);
            window.display();
        }
        //drawing gameplay
        else {
            window.clear(sf::Color::Black);
            window.draw(ceiling);
            window.draw(floor);
            while (!wall.empty()) {
                window.draw(wall.front());
                wall.pop();
            }
            window.draw(knifeSpr);
            window.draw(crosshair);
            window.draw(text);
            window.display();
        }
        frameCounter++;
        if (frameCounter>10) {
            fps=frameCounter/timeFrame.asSeconds();
            frameCounter=0;
            timeFrame=clockFrame.restart();
        }
        text.setString("FPS: "+std::to_string(fps));
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

