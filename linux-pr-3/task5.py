import random


def parse_bnf(text):
    grammar = {}
    rules = [line.split('=') for line in text.strip().split('\n')]

    for name, body in rules:
        grammar[name.strip()] = [alt.split() for alt in body.split('|')]

    return grammar


def generate_phrase(grammar, start):
    if start in grammar:
        seq = random.choice(grammar[start])
        return ''.join(generate_phrase(grammar, name) for name in seq)

    if start == 'OR':
        return '|'

    return str(start)


BNF = '''
E = V | V & E | V OR E
V = x | y | ~ V | ( E )
'''


for i in range(10):
    print(generate_phrase(parse_bnf(BNF), 'E'))
