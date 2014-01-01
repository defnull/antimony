import base
import datum

class GetBounds(base.Node):
    def __init__(self, name, x, y):
        super(GetBounds, self).__init__(name)

        self.add_datum('x', datum.FloatDatum(self, x))
        self.add_datum('y', datum.FloatDatum(self, y))

        self.add_datum('input', datum.ExpressionDatum(self, 'f1'))

        for i in 'xmin','ymin','zmin','xmax','ymax','zmax':
            self.add_datum(i, datum.FloatFunctionDatum(
                self, lambda s=i: getattr(self.input, s)))

    def get_control(self, is_child):
        import control.bounds
        return control.bounds.GetBoundsControl


class SetBounds(base.Node):
    def __init__(self, name, x, y):
        super(SetBounds, self).__init__(name)

        self.add_datum('x', datum.FloatDatum(self, x))
        self.add_datum('y', datum.FloatDatum(self, y))

        self.add_datum('input', datum.ExpressionDatum(self, 'f1'))

        for i in 'xmin','ymin','zmin','xmax','ymax','zmax':
            self.add_datum(i, datum.FloatDatum(self, 0))

        self.add_datum('output',
                       datum.ExpressionFunctionDatum(self, self.make_shape))

    def make_shape(self):
        s = self.input
        for i in 'xmin','ymin','zmin','xmax','ymax','zmax':
            setattr(s, i, getattr(self, i))
        return s

    def get_control(self, is_child):
        import control.bounds
        return control.bounds.SetBoundsControl

