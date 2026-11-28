from string import Template


def expand(template, variables):
    return Template(template).safe_substitute(variables)
