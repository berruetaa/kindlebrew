/*
 * Minimal libGL.so.1 compatibility shim for GNOME Chess on newer Kindle firmware.
 *
 * The upstream glchess ARMHF binary is hard-linked against libGL.so.1 even
 * though the Kindle port forces the 2D ChessView path. Newer Kindle firmware
 * no longer ships libGL.so.1, so the dynamic loader aborts before main().
 *
 * These symbols satisfy the binary's seven unresolved GL references. They are
 * deliberately no-ops: the 2D renderer does not call them.
 */

typedef unsigned int GLenum;
typedef int GLsizei;
typedef int GLint;
typedef void GLvoid;

void glDisableClientState(GLenum array) { (void)array; }
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    (void)mode; (void)count; (void)type; (void)indices;
}
void glEnable(GLenum cap) { (void)cap; }
void glEnableClientState(GLenum array) { (void)array; }
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    (void)type; (void)stride; (void)pointer;
}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    (void)size; (void)type; (void)stride; (void)pointer;
}
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    (void)size; (void)type; (void)stride; (void)pointer;
}
