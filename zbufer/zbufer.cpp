#define MAXDIST 1000.0  // Максимальная глубина сцены
#define MAXYLINES 600  // Высота окна
#define MAXXLINES 800  // Ширина окна
#define DEFAULT_COLOR 0xFFFFFF  // Белый цвет фона
int currentPolygonIndex = 0;  // Индекс текущего многоугольника для вывода
int output = 1;


#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>


using namespace std;


// Структура для точки в 3D пространстве
struct Point3d {
    double x, y, z;
};


// Структура ячейки Z-буфера
struct Cell {
    double z;  // Глубина текущего пикселя на экране
    int color;  // Цвет пикселя
};


// Класс описывает многоугольник
class Polygon {
public:
    int color;  // Цвет многоугольника
    vector<Point3d> points;  // Вектор для хранения вершин многоугольника

    // Конструктор, передаем ссылку на вектор вершин и цвет
    Polygon(const vector<Point3d>& pts, int c) : points(pts), color(c) {}
};


// Класс Z-буфера
class ZBuffer {
public:
    Cell* buff[MAXYLINES];  // Массив строк, где каждая строка - массив ячеек
    int sX;  // Ширина (кол-во пикселей по горизонтали)
    int sY;  // Высота (кол-во строк)

    // Конструктор инициализирует z-буфер размерами ax на ay
    ZBuffer(int ax, int ay) {
        sX = ax;
        sY = ay;

        // Для каждой строки выделяется память под массив ячеек, где каждая ячейка = 1 пикселю
        for (int i = 0; i < sY; i++) {
            buff[i] = (Cell*)malloc(sX * sizeof(Cell));
        }
        Clear();
    }

    // Деструктор освобождает память для каждой строки
    ~ZBuffer() {
        for (int i = 0; i < sY; i++) {
            free(buff[i]);
        }
    }

    // Выполняет очистку z-буфера, подготавливая его к новому рендерингу
    void Clear() {
        for (int j = 0; j < sY; j++) {
            for (int i = 0; i < sX; i++) {
                buff[j][i].z = MAXDIST;
                buff[j][i].color = DEFAULT_COLOR;
            }
        }
    }

    // Заполнение z-буфера многоугольником
    void PutPolygon(const Polygon& poly);

    // Отображение содержимого буфера
    void Show();

    // Вывод в консоль текущего состояния z-буфера
    void PrintState(const string& filename, int ymin, int ymax, int xmin, int xmax) {

        cout << "-Cout- " << endl;

        ofstream outFile(filename);
        if (!outFile.is_open()) {
            cerr << "Ошибка: не удалось открыть файл для записи!" << endl;
            return;
        }

        for (int i = ymin; i < ymax; i++) {
            for (int j = xmin; j < xmax; j++) {
                outFile << "(" << buff[i][j].z << ", " << hex << buff[i][j].color << ") ";
            }
            outFile << endl;
        }
        outFile.close();  // Закрываем файл
    }
};


// Заполнение Z-буфера цветом и глубиной
void ZBuffer::PutPolygon(const Polygon& poly) {
    int numPoints = poly.points.size();  // Определяем кол-во вершин многоугольника
    
    if ((numPoints < 3) || (numPoints > 6)) return;  // Игнорировать фигуры с количеством вершин меньше 3 или больше 6 
    
    // Массивы для хранения координат x, y всех вершин
    int* x = new int[numPoints];
    int* y = new int[numPoints];

    // Координаты преобразуются в целочисленные значения для работы в дискретной плоскости
    for (int i = 0; i < numPoints; i++) {
        x[i] = int(poly.points[i].x);
        y[i] = int(poly.points[i].y);
    }

    // Определяем минималью и максимальную коорлинаты y многоугольника, чтобы пропустить пустые строки
    int ymin = *min_element(y, y + numPoints);
    int ymax = *max_element(y, y + numPoints);

    // Для вывода в консоль
    int xmin = *min_element(x, x + numPoints);
    int xmax = *max_element(x, x + numPoints);

    // Координаты ограничиваются размерами z-буфера
    ymin = max(0, ymin);
    ymax = min(sY, ymax);

    xmin = max(0, xmin);
    ymax = min(sX, xmax);

    // Сканирование по строкам 
    for (int ysc = ymin; ysc < ymax; ysc++) {
        // Вектор для хранения пересечений текущей строки с ребрами многоугольника
        vector<pair<int, double>> intersections;  // Содержит координатц x и значение глубины

        // для каждой вершины определяется следующее ребро 
        for (int i = 0; i < numPoints; i++) {
            int j = (i + 1) % numPoints;
            if (y[i] == y[j]) continue;  // Пропускаем горизонтальные ребра, так как онм не пересекаются с текущей строкой y

            // Проверяем, пересекает ли текущее ребро строку y
            if ((y[i] < y[j] && ysc >= y[i] && ysc < y[j]) ||
                (y[i] > y[j] && ysc >= y[j] && ysc < y[i])) {
                double t_interp = double(ysc - y[i]) / (y[j] - y[i]);  // Вычисляем интерполяционный параметр t
                int x_new = x[i] + t_interp * (x[j] - x[i]);  // Координата x пересечения
                double z_new = poly.points[i].z + t_interp * (poly.points[j].z - poly.points[i].z);  // Глубина пересечения
                intersections.emplace_back(x_new, z_new);  // Найденное пересечение добавляется в вектор intersections
            }
        }

        // Сортировка пересечений по x, чтобы определить начало и конец горизонтальных отрезков
        sort(intersections.begin(), intersections.end());

        // Итерация по парам пересечений (каждый отрезок определяется двумя соседними точками)
        for (size_t k = 0; k + 1 < intersections.size(); k += 2) {
            // Сохраняются координаты x, z начала и конца для текущего отрезка
            int x1 = intersections[k].first;
            int x2 = intersections[k + 1].first;
            double z1 = intersections[k].second;
            double z2 = intersections[k + 1].second;

            // Итерация по каждому пиклелю внутри отрезка
            for (int xsc = max(0, x1); xsc < min(sX, x2); xsc++) {
                double t_interp = double(xsc - x1) / (x2 - x1);  // Интерполяционный параметр
                double z = z1 + t_interp * (z2 - z1);  // По t интерполируется значение глубины 

                // Если глубина меньше текущего значения в буфере, ячейка буфера обновляется
                if (z < buff[ysc][xsc].z) {
                    buff[ysc][xsc].z = z;  // Новая глубина
                    buff[ysc][xsc].color = poly.color;  // Новый цвет
                }
            }
        }
    }

    if (currentPolygonIndex == output) {
        PrintState("zbuffer_output.txt", ymin, ymax, xmin, xmax);
        output++;
    }

    // Очистка памяти
    delete[] x;
    delete[] y;
}

// Отображение содержимого Z-буфера
void ZBuffer::Show() {
    glBegin(GL_POINTS);  // Инициализация режима для рисования точек

    // Перебор всех пикселей
    for (int y = 0; y < sY; y++) {
        for (int x = 0; x < sX; x++) {
            int color = buff[y][x].color;  // Извлечение цвета текущего пикселя

            // Утсановка цвета в OpenGL
            glColor3ub((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);            
            glVertex2i(x, y);  // Рисование точки
        }
    }
    glEnd();  // Завершаем режим рисования точек
}



// Главная функция
int main() {
    glfwInit();  // Инициализация библиотеки GLFW
    bool spacePressed = false;  // Флаг состояния клавиши пробел

    // Создаем окно 800 на 600
    GLFWwindow* window = glfwCreateWindow(MAXXLINES, MAXYLINES, "Z-Buffer", NULL, NULL);

    // Устанавливает созданное окно как текущий контекст OpenGL
    glfwMakeContextCurrent(window);

    // Настраивает ортографическую проекцию, чтобы координаты пикселей соответствовали экранным (верхний левый угол — начало координат)
    glOrtho(0, MAXXLINES, 0, MAXYLINES, -1, 1);

    ZBuffer zb(MAXXLINES, MAXYLINES);  // Создание Z-буфера

    // Инициализация многоугольников
    vector<Polygon> polygons = {
        Polygon({{300, 260, 50}, {560, 280, -50}, {480, 340, 0}}, 0xFF0000),
        Polygon({{380, 300, -50}, {480, 381, -50}, {500, 230, 100}, {420, 220, 100}}, 0x00FF00),
        Polygon({{200, 160, 100}, {500, 400, 100}, {600, 340, 100}, {600, 250, 100}}, 0xFF0FF0),
        Polygon({{400, 200, -300}, {400, 300, 200}, {700, 300, 200}, {700, 200, -300}}, 0xFFF51F),
        Polygon({{280, 200, 100}, {290, 300, -80}, {550, 300, 100}, {530, 200, 200}, {520, 150, 150}}, 0x55555F),
    };

    

    // Главный цикл программы (рендеринг)
    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);  // Очистка буфера цвета

        zb.Clear();  // Очитска Z-буфера

        // Обработка многоугольников
        for (int i = 0; i < currentPolygonIndex; i++) {
            zb.PutPolygon(polygons[i]);
        }


        zb.Show();  // Отображаем содержимое Z-буфера

        // Обработка клавиши
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if(!spacePressed)
            {
                spacePressed = true;  // Установить флаг
                if (currentPolygonIndex < polygons.size()) {
                    zb.PutPolygon(polygons[currentPolygonIndex]);
                    currentPolygonIndex++;
                    zb.Show();
                    glfwSwapBuffers(window);

                }
            }
        }
        else {
            spacePressed = false;  // Сбросить флаг при отпускании клавиши
        }

        glfwSwapBuffers(window);  // Обновляем содержиоме окна 
        glfwPollEvents();  // Проверка событий
    }

    // Освобождение ресурсов после закрытия окна
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
