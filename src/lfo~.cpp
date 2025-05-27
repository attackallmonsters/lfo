// lfo~.cpp – Efficient LFO external for Pure Data
#include "m_pd.h"
#include <cmath>
#include <cstdlib>

static t_class *lfo_tilde_class;

typedef struct t_lfo_tilde t_lfo_tilde;
typedef float (*lfo_func_ptr)(t_lfo_tilde *);

typedef struct t_lfo_tilde
{
    t_object x_obj;
    t_float f_phase;
    t_float f_freq;
    t_float f_inc;
    t_float f_sr;
    t_float last_val;
    t_float f_offset;
    t_float f_depth;
    t_float f_shape;
    t_float f_pw;
    bool oneshot_enabled;
    lfo_func_ptr lfo_func;
    t_outlet *x_out_sig;
    t_outlet *x_out_bang;
} t_lfo_tilde;

inline float shaped_ramp(float x, float shape)
{
    if (shape == 0.0f)
        return x;
    else if (shape > 0.0f)
        return std::pow(x, 1.0f + shape * 4.0f); // convex
    else
        return 1.0f - std::pow(1.0f - x, 1.0f - shape * 4.0f); // concave
}

// --- LFO functions ---
float lfo_sine(t_lfo_tilde *x)
{
    // Sine bleibt unverändert – shape hat hier keinen Effekt
    return std::sin(x->f_phase * 2.0 * M_PI);
}

float lfo_rampup(t_lfo_tilde *x)
{
    float shaped = shaped_ramp(x->f_phase, x->f_shape);
    return 2.0f * shaped - 1.0f;
}

float lfo_rampdown(t_lfo_tilde *x)
{
    float shaped = shaped_ramp(x->f_phase, x->f_shape);
    return 1.0f - 2.0f * shaped;
}

float lfo_triangle(t_lfo_tilde *x)
{
    float p = x->f_phase * 2.0f;
    if (p < 1.0f)
        return 2.0f * shaped_ramp(p, x->f_shape) - 1.0f;
    else
        return 1.0f - 2.0f * shaped_ramp(p - 1.0f, x->f_shape);
}

float lfo_square(t_lfo_tilde *x)
{
    return (x->f_phase < x->f_pw) ? 1.0f : -1.0f;
}

float lfo_random(t_lfo_tilde *x)
{
    return x->last_val;
}

void lfo_tilde_reset(t_lfo_tilde *x)
{
    x->f_phase = 0.0f;
    outlet_bang(x->x_out_bang);
}

void lfo_tilde_setoffset(t_lfo_tilde *x, t_floatarg f)
{
    x->f_offset = f;
}

void lfo_tilde_setdepth(t_lfo_tilde *x, t_floatarg f)
{
    x->f_depth = f;
}

void lfo_tilde_setshape(t_lfo_tilde *x, t_floatarg f)
{
    x->f_shape = f;
}

void lfo_tilde_setpw(t_lfo_tilde *x, t_floatarg f)
{
    // Begrenzen auf sinnvolle Werte
    if (f < 0.01f)
        f = 0.01f;
    if (f > 0.99f)
        f = 0.99f;
    x->f_pw = f;
}

t_int *lfo_tilde_perform(t_int *w)
{
    t_lfo_tilde *x = (t_lfo_tilde *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    int n = (int)(w[3]);

    if (x->f_freq <= 0.0f)
    {
        x->f_phase = 0.0f;
        for (int i = 0; i < n; i++)
            out[i] = 0.0f;
        return (w + 4);
    }

    float phase = x->f_phase;
    float inc = x->f_inc;
    float val = 0.0f;

    for (int i = 0; i < n; i++)
    {
        if (x->lfo_func == lfo_random)
        {
            val = x->last_val;
        }
        else
        {
            x->f_phase = phase;
            val = x->lfo_func(x);
        }

        // Apply depth & offset
        out[i] = val * x->f_depth + x->f_offset;

        // Phase advance
        phase += inc;

        // Wrap
        if (phase >= 1.0f)
        {
            phase -= 1.0f;

            // Nur für random: neuen Sample vorbereiten
            if (x->lfo_func == lfo_random)
                x->last_val = 2.0f * ((float)rand() / RAND_MAX) - 1.0f;

            outlet_bang(x->x_out_bang);
        }
    }

    x->f_phase = phase;
    return (w + 4);
}

// --- DSP setup ---
void lfo_tilde_dsp(t_lfo_tilde *x, t_signal **sp)
{
    x->f_sr = sp[0]->s_sr;
    x->f_inc = x->f_freq / x->f_sr;
    dsp_add(lfo_tilde_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
}

// --- Set frequency ---
void lfo_tilde_setfreq(t_lfo_tilde *x, t_floatarg f)
{
    x->f_freq = fmax(0.0f, f);
    x->f_inc = (x->f_freq > 0.0f) ? x->f_freq / x->f_sr : 0.0f;
}

// --- Set type using function table ---
lfo_func_ptr lfo_table[] = {
    lfo_sine,
    lfo_rampup,
    lfo_rampdown,
    lfo_triangle,
    lfo_square,
    lfo_random};

void lfo_tilde_settype(t_lfo_tilde *x, t_floatarg f)
{
    int type = static_cast<int>(f);
    if (type >= 0 && type < 6)
        x->lfo_func = lfo_table[type];
}

// --- Constructor ---
void *lfo_tilde_new(t_floatarg f)
{
    t_lfo_tilde *x = (t_lfo_tilde *)pd_new(lfo_tilde_class);
    x->f_freq = f > 0 ? f : 1.0f;
    x->f_phase = 0.0f;
    x->last_val = 0.0f;
    x->f_sr = 44100.0f;
    x->f_shape = 0.0f;
    x->f_offset = 0.0f;
    x->f_depth = 1.0f;
    x->f_pw = 0.5f;
    x->f_inc = x->f_freq / x->f_sr;
    x->lfo_func = lfo_sine;

    x->x_out_sig = outlet_new(&x->x_obj, &s_signal);
    x->x_out_bang = outlet_new(&x->x_obj, &s_bang);

    return (void *)x;
}

// --- Setup ---
extern "C"
{
    void lfo_tilde_setup(void)
    {
        lfo_tilde_class = class_new(gensym("lfo~"),
                                    (t_newmethod)lfo_tilde_new,
                                    0, sizeof(t_lfo_tilde),
                                    CLASS_DEFAULT, A_DEFFLOAT, 0);

        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_dsp, gensym("dsp"), A_CANT, 0);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_setfreq, gensym("freq"), A_DEFFLOAT, A_NULL);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_settype, gensym("type"), A_DEFFLOAT, A_NULL);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_setoffset, gensym("offset"), A_DEFFLOAT, A_NULL);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_setdepth, gensym("depth"), A_DEFFLOAT, A_NULL);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_setshape, gensym("shape"), A_NULL);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_setpw, gensym("pw"), A_DEFFLOAT, A_NULL);
        class_addmethod(lfo_tilde_class, (t_method)lfo_tilde_reset, gensym("reset"), A_NULL);
    }
}
