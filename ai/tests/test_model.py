from libras_ai.model import build_mlp


def test_mlp_has_five_inputs_and_five_outputs():
    model = build_mlp(input_features=5, num_classes=5)
    assert model.input_shape == (None, 5)
    assert model.output_shape == (None, 5)


def test_mlp_class_count_is_configurable():
    model = build_mlp(input_features=5, num_classes=8)
    assert model.output_shape == (None, 8)
