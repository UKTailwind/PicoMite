/* maze3.c - Maze2.bas in C: random perfect mazes, animated construction and
   depth-first solving.  Any key stops between mazes, CTRL-C at once. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <picomite/system.h>
#include <picomite/graphics.h>
#include <picomite/input.h>

#define COLS 39
#define ROWS 25
#define TOTAL (COLS * ROWS)

static const int mazedx[4] = {1, 0, -1, 0}, mazedy[4] = {0, 1, 0, -1};
static const int mazeflag[4] = {1, 2, 4, 8}, mazeopposite[4] = {4, 8, 1, 2};

static int walls[TOTAL], seen[TOTAL], stack[TOTAL];
static int side, ox, oy, fn;

/* Box 0,0,MM.HRES,oy-2,0,0,RGB(BLACK): Text MM.HRES\2,oy\2,s$,"CM",fn,2,colour */
static void header(const char *s, int colour)
{
    pm_draw_box(0, 0, pm_hres(), oy - 2, 0, 0, PM_BLACK);
    pm_text(pm_hres() / 2, oy / 2, s, "CM", fn, 2, colour, PM_DEFAULT);
}

static void walk(int a, int b, int colour)
{
    double ax = ox + (a % COLS) * side + side / 2.0, ay = oy + (a / COLS) * side + side / 2.0;
    double bx = ox + (b % COLS) * side + side / 2.0, by = oy + (b / COLS) * side + side / 2.0;
    double px = ax, py = ay;
    for (int frame = 1; frame <= 9; frame++)
    {
        double qx = ax + (bx - ax) * frame / 9, qy = ay + (by - ay) * frame / 9;
        pm_draw_box((int)(px - 1.5), (int)(py - 1.5), 5, 5, 0, 0, colour); /* (BASIC rounds) */
        pm_draw_box((int)(qx - 1.5), (int)(qy - 1.5), 5, 5, 0, 0, PM_YELLOW);
        px = qx, py = qy;
        pm_pause(12);
    }
}

static void route(int a, int b, int colour)
{
    pm_line(ox + (a % COLS) * side + side / 2, oy + (a / COLS) * side + side / 2,
            ox + (b % COLS) * side + side / 2, oy + (b / COLS) * side + side / 2, 3, colour);
}

int main(void)
{
    int number = 0, choices[4];
    char s[64];

// for HDMI only: pm_mode(3);
    side = pm_hres() / (COLS + 1);
    ox = (pm_hres() - COLS * side) / 2;
    oy = (pm_vres() - ROWS * side) / 2 + side / 2;
    fn = pm_hres() > 320 ? 1 : 7;
    srand((unsigned)pm_timer_us());

    do
    {
        int cur, top, dest, count, decisions;
        number++;
        memset(walls, 0, sizeof walls);
        memset(seen, 0, sizeof seen);
        pm_cls(PM_BLACK);
        snprintf(s, sizeof s, "MAZE %d - BUILDING...", number);
        pm_text(pm_hres() / 2, oy / 2, s, "CM", fn, 2, PM_WHITE, PM_DEFAULT);
        for (int i = 0; i <= COLS; i++)
            pm_line(ox + i * side, oy, ox + i * side, oy + ROWS * side, 1, PM_WHITE);
        for (int i = 0; i <= ROWS; i++)
            pm_line(ox, oy + i * side, ox + COLS * side, oy + i * side, 1, PM_WHITE);

        /* build: depth-first with backtracking */
        cur = 0, top = 0, stack[0] = 0, seen[0] = 1;
        for (;;)
        {
            int x = cur % COLS, y = cur / COLS, d;
            count = 0;
            for (d = 0; d < 4; d++)
            {
                int nx = x + mazedx[d], ny = y + mazedy[d];
                if (nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && !seen[ny * COLS + nx])
                    choices[count++] = d;
            }
            if (count > 0)
            {
                d = choices[rand() % count];
                dest = cur + mazedx[d] + mazedy[d] * COLS;
                walls[cur] |= mazeflag[d];
                walls[dest] |= mazeopposite[d];
                if (mazedx[d])
                {
                    int nx = mazedx[d] < 0 ? ox + x * side : ox + (x + 1) * side;
                    pm_line(nx, oy + y * side + 1, nx, oy + (y + 1) * side - 1, 1, PM_BLACK);
                }
                else
                {
                    int ny = mazedy[d] < 0 ? oy + y * side : oy + (y + 1) * side;
                    pm_line(ox + x * side + 1, ny, ox + (x + 1) * side - 1, ny, 1, PM_BLACK);
                }
                stack[++top] = dest;
                seen[dest] = 1, cur = dest;
                pm_pause(3);
            }
            else
            {
                if (top == 0)
                    break;
                cur = stack[--top];
            }
        }
        pm_line(ox, oy + 1, ox, oy + side - 1, 4, PM_GREEN);
        pm_line(ox + COLS * side, oy + (ROWS - 1) * side + 1, ox + COLS * side, oy + ROWS * side - 1, 4, PM_YELLOW);
        snprintf(s, sizeof s, "MAZE %d - CHOICES: 0", number);
        header(s, PM_CYAN);
        pm_text(pm_hres() / 2, pm_vres() - 8, "CYAN: EXPLORED   BLUE: DEAD END   YELLOW: EXPLORER", "CM", fn, 1, PM_WHITE, PM_DEFAULT);

        /* solve: depth-first, counting the decisions */
        memset(seen, 0, sizeof seen);
        cur = 0, top = 0, stack[0] = 0, seen[0] = 1, decisions = 0;
        pm_draw_box(ox + side / 2 - 2, oy + side / 2 - 2, 5, 5, 0, 0, PM_YELLOW);
        pm_pause(500);
        while (cur != TOTAL - 1)
        {
            count = 0;
            for (int d = 0; d < 4; d++)
                if (walls[cur] & mazeflag[d])
                {
                    dest = cur + mazedx[d] + mazedy[d] * COLS;
                    if (!seen[dest])
                        choices[count++] = d;
                }
            if (count > 0)
            {
                int d;
                if (count > 1)
                { /* a decision: two or more unexplored directions */
                    decisions++;
                    snprintf(s, sizeof s, "MAZE %d - CHOICES: %d", number, decisions);
                    header(s, PM_CYAN);
                    pm_pause(250);
                }
                d = choices[rand() % count];
                dest = cur + mazedx[d] + mazedy[d] * COLS;
                stack[++top] = dest, seen[dest] = 1;
                walk(cur, dest, PM_CYAN);
            }
            else
            {
                dest = stack[--top];
                walk(cur, dest, PM_BLUE);
            }
            cur = dest;
        }
        for (int i = 1; i <= top; i++)
        { /* the route found: the search stack */
            route(stack[i - 1], stack[i], PM_GREEN);
            pm_pause(25);
        }
        snprintf(s, sizeof s, "EXIT FOUND!  %d CHOICES", decisions);
        header(s, PM_GREEN);
        pm_pause(3000);
    } while (pm_inkey() < 0);
    return 0;
}
