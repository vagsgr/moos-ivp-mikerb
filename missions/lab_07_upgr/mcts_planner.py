#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ===========================================================================
#  mcts_planner.py  --  MCTS Orienteering planner gia to Lab07_upgr
#  NAME: Evangelos Siatis / MIT 2.680 minicourse
#
#  [ΣΗΜ] To provlima DEN einai pia TSP (perase apo OLA me elaxisto mikos).
#    Yparxei BUDGET (600s underway * 1.2 m/s = 720m ana tour). Ara to zitoumeno
#    einai: MEGISTOPOIISE posa simeia tha proladoume mesa sto budget = to klasiko
#    ORIENTEERING PROBLEM. Greedy nearest-neighbor xanei (ksodevei budget gia
#    makrina clusters). Edw lynoume me MCTS (UCT).
#
#  [ΣΗΜ] Kathara Python stdlib -- KAMIA exartisi (oxi numpy/scipy). Trexei kai
#    autonoma gia debug:   python3 mcts_planner.py in.txt out.txt
#    Self-test (MCTS vs greedy):  python3 mcts_planner.py --selftest
#
#  ---- Symvolaio I/O (to pGenPathMCTS C++ grafei to infile, diavazei to outfile) ----
#  INPUT (infile), grammes:
#     start <x> <y>          # trexousa thesi oximatos (arxi diadromis)
#     budget <meters>        # mikos collecting-path pou epitrepetai (720 default)
#     point <id> <x> <y>     # ena diathesimo (mi-diekdikimeno) simeio. Polla.
#  OUTPUT (outfile), grammes me ti SEIRA episkepsis:
#     # count=<K> length=<L>
#     <id> <x> <y>
#     ...
# ===========================================================================

import sys
import math
import random

# ---------------------------------------------------------------------------
# ----- parametroi MCTS (mporoun na rythmistoun apo CLI/env, alla defaults edw) -----
#90%:
#    διάλεξε το κοντινότερο εφικτό σημείο
#10%:
#    διάλεξε τυχαίο εφικτό σημείο
# ---------------------------------------------------------------------------


ITERS  = 1500     # posa MCTS rollouts (gia 50-100 simeia: <1s)
UCT_C  = 1.4      # syntelestis exploration sto UCT
EPS    = 0.10     # epsilon-greedy mesa sto rollout (0=kathara greedy, 1=tyxaio)


#Υπολογίζει την ευκλείδεια απόσταση
def dist(ax, ay, bx, by):
    return math.hypot(ax - bx, ay - by)


# ---------------------------------------------------------------------------
#  GREEDY nearest-neighbor (vaseline gia sygkrisi + gia rollout-policy)
#  Epistrefei (seira indices, mikos) mexri na teleiosei to budget.
# ---------------------------------------------------------------------------
def greedy_path(pts, sx, sy, budget):
    n = len(pts)
    used = [False] * n
    seq = []
    cx, cy = sx, sy
    length = 0.0
    while True:
        best = -1
        best_d = 0.0
        for i in range(n):
            if used[i]:
                continue
            d = dist(cx, cy, pts[i][1], pts[i][2])
            if length + d > budget:        # den xwraei sto budget
                continue
            if best < 0 or d < best_d:
                best, best_d = i, d
        if best < 0:
            break
        used[best] = True
        seq.append(best)
        length += best_d
        cx, cy = pts[best][1], pts[best][2]
    return seq, length


# ---------------------------------------------------------------------------
#  Rollout (playout) politiki: epsilon-greedy nearest. Apo dosmeni katastasi
#  synexizei na prosthetei efikta simeia mexri na min xwraei tipota. Epistrefei
#  to PLITHOS simeion pou mazeyei (= reward).
#Το rollout δεν κινεί πραγματικά το όχημα. Είναι μόνο εσωτερική προσομοίωση πιθανής μελλοντικής διαδρομής.
# ---------------------------------------------------------------------------
def rollout(pts, used, cx, cy, length, budget):
    used = used[:]               # topiko antigrafo
    seq = []                     # [ΣΗΜ] kratame ti SEIRA pou pige to rollout
    while True:
        feas = []
        for i in range(len(pts)):
            if used[i]:
                continue
            d = dist(cx, cy, pts[i][1], pts[i][2])
            if length + d <= budget:
                feas.append((d, i))
        if not feas:
            break
        if random.random() < EPS:
            d, i = random.choice(feas)
        else:
            d, i = min(feas, key=lambda t: t[0])   # plisiestero
        used[i] = True
        length += d
        cx, cy = pts[i][1], pts[i][2]
        seq.append(i)
    return seq, length


# ---------------------------------------------------------------------------
#  LOCAL IMPROVEMENT: 2-opt (kontynei to path) + insertion (gemizei to budget
#  pou eleftherothike). Edw krivetai to pragmatiko kerdos sto orienteering:
#  oso pio konto to path, toso perissotera simeia xwrane sto idio budget.
# ---------------------------------------------------------------------------
def path_length(pts, sx, sy, seq):
    if not seq:
        return 0.0
    length = dist(sx, sy, pts[seq[0]][1], pts[seq[0]][2])
    for k in range(len(seq) - 1):
        a, b = seq[k], seq[k + 1]
        length += dist(pts[a][1], pts[a][2], pts[b][1], pts[b][2])
    return length


def two_opt(pts, sx, sy, seq):
    # Klasiko 2-opt: anastrefe tmimata an meiwnetai to synoliko mikos.
    # (To start (sx,sy) einai statheri arxi, den anastrefetai.)
    if len(seq) < 3:
        return seq, path_length(pts, sx, sy, seq)
    seq = seq[:]
    improved = True
    while improved:
        improved = False
        for i in range(len(seq) - 1):
            # proigoumeni thesi: an i==0 -> start, alliws to simeio seq[i-1]
            ax, ay = (sx, sy) if i == 0 else (pts[seq[i - 1]][1], pts[seq[i - 1]][2])
            for j in range(i + 1, len(seq)):
                bx, by = pts[seq[i]][1], pts[seq[i]][2]      # arxi tmimatos
                cx, cy = pts[seq[j]][1], pts[seq[j]][2]      # telos tmimatos
                # akmi pou fevgei apo to telos j (an yparxei epomeno)
                if j + 1 < len(seq):
                    dx, dy = pts[seq[j + 1]][1], pts[seq[j + 1]][2]
                    old = dist(ax, ay, bx, by) + dist(cx, cy, dx, dy)
                    new = dist(ax, ay, cx, cy) + dist(bx, by, dx, dy)
                else:
                    old = dist(ax, ay, bx, by)
                    new = dist(ax, ay, cx, cy)
                if new + 1e-9 < old:
                    seq[i:j + 1] = seq[i:j + 1][::-1]
                    improved = True
    return seq, path_length(pts, sx, sy, seq)


def insertion(pts, sx, sy, seq, budget):
    # Greedy cheapest-insertion: vale simeia pou DEN einai sto path, sti thesi me
    # to mikrotero epipleon kostos, oso to synoliko mikos paramenei <= budget.
    # Eξετάζει σημεία που δεν βρίσκονται ήδη στη διαδρομή και προσπαθεί να τα τοποθετήσει στη θέση με το μικρότερο πρόσθετο κόστος.
    in_path = [False] * len(pts)
    for i in seq:
        in_path[i] = True
    cur_len = path_length(pts, sx, sy, seq)
    improved = True
    while improved:
        improved = False
        best_u = -1
        best_pos = -1
        best_add = None
        for u in range(len(pts)):
            if in_path[u]:
                continue
            ux, uy = pts[u][1], pts[u][2]
            # dokimase kathe thesi 0..len (0 = prin to prwto)
            for pos in range(len(seq) + 1):
                ax, ay = (sx, sy) if pos == 0 else (pts[seq[pos - 1]][1], pts[seq[pos - 1]][2])
                if pos == len(seq):
                    add = dist(ax, ay, ux, uy)            # sto telos: mono nea akmi
                else:
                    nx, ny = pts[seq[pos]][1], pts[seq[pos]][2]
                    add = dist(ax, ay, ux, uy) + dist(ux, uy, nx, ny) - dist(ax, ay, nx, ny)
                if best_add is None or add < best_add:
                    best_add, best_u, best_pos = add, u, pos
        if best_u >= 0 and cur_len + best_add <= budget:
            seq.insert(best_pos, best_u)
            in_path[best_u] = True
            cur_len += best_add
            improved = True
    return seq, cur_len


def improve(pts, sx, sy, seq, budget, rounds=3):
    # Enallaks 2-opt (kontynei) + insertion (gemizei), merikoi gyroi.
    best = seq[:]
    best_len = path_length(pts, sx, sy, best)
    for _ in range(rounds):
        seq2, _ = two_opt(pts, sx, sy, best)
        seq2, len2 = insertion(pts, sx, sy, seq2, budget)
        if len(seq2) > len(best) or (len(seq2) == len(best) and len2 < best_len):
            best, best_len = seq2, len2
        else:
            break
    return best, best_len


# ---------------------------------------------------------------------------
#  MCTS node. Kathe node = katastasi meta apo mia akolouthia epilegmenon simeion.
# ---------------------------------------------------------------------------
class Node:
    __slots__ = ('seq', 'used', 'cx', 'cy', 'length', 'untried', 'children', 'N', 'W')

    def __init__(self, seq, used, cx, cy, length, untried):
        self.seq = seq            # list[int] = indices pou exoun epilegei
        self.used = used          # list[bool]
        self.cx = cx
        self.cy = cy
        self.length = length      # metra pou exoun ksodeftei
        self.untried = untried    # list[int] efikta epomena pou DEN exoun expandaristei
        self.children = {}        # action(idx) -> Node
        self.N = 0                # episkepseis
        self.W = 0.0              # athroisma reward


def feasible_actions(pts, used, cx, cy, length, budget):
    out = []
    for i in range(len(pts)):
        if used[i]:
            continue
        if length + dist(cx, cy, pts[i][1], pts[i][2]) <= budget:
            out.append(i)
    return out


def uct_select(node):
    # epilexe paidi me megisto UCT
    best_child = None
    best_val = -1.0
    logN = math.log(node.N + 1)
    for a, ch in node.children.items():
        if ch.N == 0:
            return ch
        exploit = ch.W / ch.N
        explore = UCT_C * math.sqrt(logN / ch.N)
        val = exploit + explore
        if val > best_val:
            best_val = val
            best_child = ch
    return best_child


def mcts(pts, sx, sy, budget, iters=ITERS):
    n = len(pts)
    if n == 0:
        return [], 0.0

    root_used = [False] * n
    root_untried = feasible_actions(pts, root_used, sx, sy, 0.0, budget)
    root = Node([], root_used, sx, sy, 0.0, root_untried)

    # Krata to KALYTERO pliris path pou eida pote (count, tie-break: mikrotero mikos).
    # (To kalytero path mporei na vrethei se rollout, oxi mono sto dentro.)
    best_seq = []
    best_count = -1
    best_len = 0.0

    def consider(seq, length):
        nonlocal best_seq, best_count, best_len
        c = len(seq)
        if c > best_count or (c == best_count and length < best_len):
            best_seq, best_count, best_len = seq[:], c, length

    # [ΣΗΜ] FLOOR: ksekina me ti greedy lysi (+ veltiwsi). Etsi to MCTS DEN
    #   mporei pote na vgei xeirotero apo to greedy. Apo ekei kai pera psaxnei.
    gseq, _ = greedy_path(pts, sx, sy, budget)
    gseq, glen = improve(pts, sx, sy, gseq, budget)
    consider(gseq, glen)

    for it in range(iters):
        node = root
        path_nodes = [node]

        # --- SELECTION: katevaine oso den exeis untried kai exeis paidia ---
        while not node.untried and node.children:
            node = uct_select(node)
            path_nodes.append(node)

        # --- EXPANSION: an exeis untried, fty kse ena paidi ---
        if node.untried:
            a = node.untried.pop(random.randrange(len(node.untried)))
            px, py = pts[a][1], pts[a][2]
            d = dist(node.cx, node.cy, px, py)
            child_used = node.used[:]
            child_used[a] = True
            child_seq = node.seq + [a]
            child_len = node.length + d
            child_untried = feasible_actions(pts, child_used, px, py, child_len, budget)
            child = Node(child_seq, child_used, px, py, child_len, child_untried)
            node.children[a] = child
            node = child
            path_nodes.append(node)

        # --- SIMULATION (rollout) apo to node ---
        # [ΣΗΜ] To rollout epistrefei OLOKLIRI tin epektasi. To pliris efikto
        #   path = (tree-path tou node) + (rollout). Auto einai pou kratame.
        roll_seq, roll_len = rollout(pts, node.used, node.cx, node.cy,
                                     node.length, budget)
        full_seq = node.seq + roll_seq
        reward = len(full_seq)
        consider(full_seq, roll_len)
        # [ΣΗΜ] Kathe N rollouts, kane local-improve to trexon best (kontynei +
        #   gemizei) -> to budget pou eleftherwnetai xwraei perissotera simeia.
        if (it + 1) % 250 == 0 and best_seq:
            iseq, ilen = improve(pts, sx, sy, best_seq, budget)
            consider(iseq, ilen)

        # --- BACKPROP ---
        for nd in path_nodes:
            nd.N += 1
            nd.W += reward

    # Teliko local-improve sto kalytero pou vrethike.
    best_seq, best_len = improve(pts, sx, sy, best_seq, budget)

    # Epistrefoume to kalytero PLIRES path pou eidame (oxi apla to "robust child"),
    # giati mas noiazei i megisti kalypsi sti synoliki diadromi.
    # Metatrope se (id,x,y) tuples me ti seira.
    ordered = [pts[i] for i in best_seq]
    return ordered, best_len


# ---------------------------------------------------------------------------
#  I/O
# ---------------------------------------------------------------------------
def read_input(path):
    sx = sy = 0.0
    budget = 720.0
    pts = []          # list of (id, x, y)
    with open(path) as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == 'start':
                sx, sy = float(t[1]), float(t[2])
            elif t[0] == 'budget':
                budget = float(t[1])
            elif t[0] == 'point':
                pts.append((t[1], float(t[2]), float(t[3])))
    return sx, sy, budget, pts


def write_output(path, ordered, length):
    with open(path, 'w') as f:
        f.write("# count=%d length=%.2f\n" % (len(ordered), length))
        for (pid, x, y) in ordered:
            f.write("%s %.3f %.3f\n" % (pid, x, y))


# ---------------------------------------------------------------------------
#  Self-test: tyxaia simeia, sygkrini MCTS vs greedy se kalypsi (count).
# ---------------------------------------------------------------------------
def selftest(trials=8, npts=60, budget=720.0, seed=1, iters=ITERS):
    random.seed(seed)
    print("Self-test: MCTS vs Greedy  (npts=%d budget=%.0f iters=%d trials=%d seed=%d)" %
          (npts, budget, iters, trials, seed))
    print("-" * 52)
    tot_m = tot_g = 0
    for t in range(trials):
        # idia geometria me to lab: X in [-25,200], Y in [-175,-25]
        pts = [(str(i + 1),
                random.uniform(-25, 200),
                random.uniform(-175, -25)) for i in range(npts)]
        sx, sy = random.uniform(-25, 200), random.uniform(-175, -25)

        gseq, glen = greedy_path(pts, sx, sy, budget)
        gcount = len(gseq)

        mordered, mlen = mcts(pts, sx, sy, budget, iters=iters)
        mcount = len(mordered)

        tot_m += mcount
        tot_g += gcount
        flag = "  <-- MCTS kalytero" if mcount > gcount else \
               ("  (isopalia)" if mcount == gcount else "  <-- GREEDY kalytero?!")
        print("trial %d:  greedy=%2d (%.0fm)   mcts=%2d (%.0fm)%s" %
              (t + 1, gcount, glen, mcount, mlen, flag))
    print("-" * 52)
    print("SYNOLO simeion:  greedy=%d   mcts=%d   (+%d, +%.1f%%)" %
          (tot_g, tot_m, tot_m - tot_g,
           100.0 * (tot_m - tot_g) / max(1, tot_g)))


def main():
    args = sys.argv[1:]
    if args and args[0] == '--selftest':
        # [Lab07_upgr] Dexetai proairetika key=value orismata, px:
        #   python3 mcts_planner.py --selftest npts=100 trials=12 iters=2000 budget=500 seed=7
        # To 'iters' edw einai TOPIKO -> DEN epireazei to mission (global ITERS).
        opts = {}
        allowed = {'trials': int, 'npts': int, 'iters': int, 'seed': int, 'budget': float}
        for a in args[1:]:
            if '=' not in a:
                sys.stderr.write("agnow arg (theloume key=value): %s\n" % a)
                continue
            k, v = a.split('=', 1)
            k = k.strip().lower()
            if k not in allowed:
                sys.stderr.write("agnwsto key '%s' (epitrepta: %s)\n" %
                                 (k, ", ".join(sorted(allowed))))
                continue
            try:
                opts[k] = allowed[k](v)
            except ValueError:
                sys.stderr.write("mi-egkyri timi gia %s: %s\n" % (k, v))
        selftest(**opts)
        return
    if len(args) < 2:
        sys.stderr.write("usage: mcts_planner.py <infile> <outfile>\n")
        sys.stderr.write("       mcts_planner.py --selftest [npts=N] [trials=N] "
                         "[iters=N] [budget=M] [seed=N]\n")
        sys.exit(1)
    sx, sy, budget, pts = read_input(args[0])
    ordered, length = mcts(pts, sx, sy, budget)
    write_output(args[1], ordered, length)


if __name__ == '__main__':
    main()
