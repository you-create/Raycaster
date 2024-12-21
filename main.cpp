#include <array>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
//#include <iostream>
#include <cmath>

int main()
{
    sf::RenderWindow window(sf::VideoMode(1200, 600), "Ray-Caster", sf::Style::Titlebar | sf::Style::Close);

    //colours
    sf::Color Grey(180, 180, 180);
    sf::Color DarkGreen(70, 200, 40);

    //making grid
    sf::VertexArray grid(sf::Lines, 40);
    float gridX{0};
    float gridY{0};
    //vertical lines
    for (int i=0;i<18; i++) {
        grid[i].position = sf::Vector2f(gridX, 0);
        i++;
        grid[i].position = sf::Vector2f(gridX, 600);
        gridX += 75;
    }
    //horizontal lines
    for (int i=20;i<40;i++) {
        grid[i].position = sf::Vector2f(0, gridY);
        i++;
        grid[i].position = sf::Vector2f(600, gridY);
        gridY += 75;
    }
    //tiles
    sf::RectangleShape tile;
    tile.setSize(sf::Vector2f(75, 75));
    tile.setFillColor(Grey);
    //map
    int map[8][8] = {
        {1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,1},
        {1,0,0,0,0,1,0,1},
        {1,1,1,0,0,0,0,1},
        {1,0,0,0,0,1,0,1},
        {1,0,0,0,0,1,0,1},
        {1,0,0,0,0,1,0,1},
        {1,1,1,1,1,1,1,1},
    };

    //player
    sf::Vector3f p(300.f,300.f,0.f); //position and direction (x, y, theta in rad)
    sf::ConvexShape player;
    player.setPointCount(3);
    player.setPoint(0, sf::Vector2f(p.x,p.y));//drawing as triangle
    player.setPoint(1, sf::Vector2f(p.x-20,p.y-5.36f));
    player.setPoint(2, sf::Vector2f(p.x-20,p.y+5.36f));
    player.setFillColor(sf::Color::Green);
    player.setOrigin(p.x,p.y);

    //ray
    constexpr int fov=50; //field of view in degrees
    std::array<sf::VertexArray, fov> ray;
    for (int i=0;i<fov;i++) {
        ray[i]=sf::VertexArray(sf::PrimitiveType::Lines,2);
        ray[i][0].color=sf::Color::Red;
        ray[i][1].color=sf::Color::Red;
    }
    std::array<float, fov> rl;

    //wall rectangles
    std::array<sf::RectangleShape, fov> wall;

    //algorithm use
    std::array<int, fov> side; //used for algo and to select wall colour

    sf::Vector2f scaledPos; //scaled position
    sf::Vector2i truncatedPos; //scaled position truncated to int
    sf::Vector2f internalPos; //scaled position - truncated position (position within current square)
    sf::Vector2f n; //normalised direction vector
    sf::Vector2f distX;
    sf::Vector2f distY;
    sf::Vector2f unit; //1 or -1


    sf::Clock clock;

    while (window.isOpen())
    {
        window.setFramerateLimit(60);
        sf::Time dt=clock.restart(); //delta time

        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type==sf::Event::Closed) window.close();
            if (event.type==sf::Event::KeyPressed&&event.key.code==sf::Keyboard::Escape) window.close();
        }

        //keyboard control
        p.z=player.getRotation()*M_PI/180;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) { //forwards
            p.x+=cos(p.z)*100*dt.asSeconds(); p.y+=sin(p.z)*100*dt.asSeconds();
        } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) { //backwards
            p.x-=cos(p.z)*100*dt.asSeconds(); p.y-=sin(p.z)*100*dt.asSeconds();}
        player.setPosition(p.x, p.y);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) { //rotate ccw
            player.setRotation(player.getRotation()-200*dt.asSeconds());
        } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) { //rotate cw
            player.setRotation(player.getRotation()+200*dt.asSeconds());}

        for (int i=0;i<fov;i++) {
            ray[i][0].position={p.x,p.y};
        }

        //DDA algo. checks grid+1 in direction of next nearest intercept
        for (int i=0;i<fov;i++) {
            n.x=cos(p.z+(-(fov/2)+i)*M_PI/180);
            n.y=sin(p.z+(-(fov/2)+i)*M_PI/180);
            if (n.y==0)n.y+=0.00001; //avoiding division by zero

            scaledPos={8*p.x/600,8*p.y/600}; //scaled position
            truncatedPos={static_cast<int>(scaledPos.x),static_cast<int>(scaledPos.y)}; //scaled position cast to int (map use)
            internalPos={scaledPos.x-static_cast<float>(truncatedPos.x),scaledPos.y-static_cast<float>(truncatedPos.y)}; //internal position in square

            if (n.x>0) {
                distX.x=(1-internalPos.x)/n.x; //dist to first ver intercept
                unit.x=1;
                distX.y=1/n.x; //dist between consecutive ver intercepts
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
                distY.x=internalPos.y/n.y; //dist to first hor intercept
                unit.y=-1;
                distY.y=1/n.y; //dist between consecutive hor intercepts
            }

            while (true) {
                if (abs(distX.x)<abs(distY.x)) { //distance to next ver intercept shorter
                    side[i]=1;
                    distX.x+=distX.y;
                    truncatedPos.x+=unit.x;
                } else {
                    side[i]=0;
                    distY.x+=distY.y;
                    truncatedPos.y+=unit.y;
                }
                if (map[truncatedPos.x][truncatedPos.y]!=0)break;
            }

            if (side[i]==1) {
                ray[i][1].position={p.x+n.x*600*abs(distX.x-distX.y)/8,p.y+n.y*600*abs(distX.x-distX.y)/8};
            } else ray[i][1].position={p.x+n.x*600*abs(distY.x-distY.y)/8,p.y+n.y*600*abs(distY.x-distY.y)/8};

            rl[i]=sqrt(pow((ray[i][1].position.x-ray[i][0].position.x),2)+pow((ray[i][1].position.y-ray[i][0].position.y),2));
        }

        for (int i=0;i<fov;i++) {
            side[i]==1 ? wall[i].setFillColor(sf::Color::Green) : wall[i].setFillColor(DarkGreen);
            wall[i].setSize(sf::Vector2f(600/fov, 30000/(rl[i]*cos(abs(-(fov/2)+i)*M_PI/180))));
            wall[i].setPosition((637.5+i*(600/fov)),300);
            wall[i].setOrigin(37.5,wall[i].getSize().y/2);
        }

        window.clear(sf::Color::Black);
        for(int i=0;i<8;i++) {
            for(int j=0;j<8;j++){
                if(map[i][j] == 1)
                {
                    tile.setPosition(static_cast<float>(i)*75,static_cast<float>(j)*75);
                    window.draw(tile);
                }
            }
        }
        window.draw(grid);
        window.draw(player);
        for (int i=0;i<fov;i++) {
            window.draw(ray[i]);
            window.draw(wall[i]);
        }
        window.display();
        //std::cout<<(1/dt.asSeconds())<<'\n'; //fps
    }
    return 0;
}

//collisions, shadows, textures, map loading/gen, up/down, gameplay

