{% extends "Base_s.c" %}

{% block LocalIncludeFiles %}
{{ block.super }}
#include "oyranos_io.h"
#include "oyranos_icc.h"
#include "oyConfig_s_.h"
#include "oyProfileTag_s_.h"
{% endblock %}
