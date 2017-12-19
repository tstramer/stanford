from learning.user_convo_solver import UserConvoSolver
from services.q_table import set_q_table

solver = UserConvoSolver()

def update_q_table():
	Q = solver.solve()
	set_q_table(Q)