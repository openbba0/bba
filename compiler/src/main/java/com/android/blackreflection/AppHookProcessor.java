package com.android.blackreflection;

import com.google.auto.service.AutoService;
import com.squareup.javapoet.ClassName;
import com.squareup.javapoet.JavaFile;
import com.squareup.javapoet.MethodSpec;
import com.squareup.javapoet.ParameterizedTypeName;
import com.squareup.javapoet.TypeName;
import com.squareup.javapoet.TypeSpec;

import java.io.IOException;
import java.util.Comparator;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.Processor;
import javax.annotation.processing.RoundEnvironment;
import javax.annotation.processing.SupportedAnnotationTypes;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.Element;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.TypeElement;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.tools.Diagnostic;

/**
 * Writes {@code AppHookRegistry} from every concrete {@code AppHook} subclass in this compilation.
 */
@AutoService(Processor.class)
@SupportedAnnotationTypes("*")
public class AppHookProcessor extends AbstractProcessor {

    private static final String APP_HOOK = "com.android.bb.hook.guest.AppHook";
    private static final String PACKAGE = "com.android.bb.hook.guest";
    private static final String REGISTRY = "AppHookRegistry";

    private boolean collected;

    @Override
    public SourceVersion getSupportedSourceVersion() {
        return SourceVersion.latestSupported();
    }

    @Override
    public boolean process(Set<? extends TypeElement> annotations, RoundEnvironment roundEnv) {
        if (roundEnv.processingOver() || collected) {
            return false;
        }
        collected = true;
        Set<String> hooks = new TreeSet<>(Comparator.naturalOrder());
        for (Element element : roundEnv.getRootElements()) {
            if (element.getKind() != ElementKind.CLASS) {
                continue;
            }
            TypeElement type = (TypeElement) element;
            if (!isConcreteAppHook(type)) {
                continue;
            }
            if (!hasPublicNoArgConstructor(type)) {
                processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,
                        "AppHook subclass needs a public no-arg constructor", type);
                continue;
            }
            hooks.add(type.getQualifiedName().toString());
        }
        try {
            registry(hooks).writeTo(processingEnv.getFiler());
        } catch (IOException e) {
            processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,
                    "failed to generate AppHookRegistry: " + e.getMessage());
        }
        return false;
    }

    private boolean isConcreteAppHook(TypeElement type) {
        if (type.getModifiers().contains(Modifier.ABSTRACT)) {
            return false;
        }
        if (!type.getModifiers().contains(Modifier.PUBLIC)) {
            return false;
        }
        TypeMirror superType = type.getSuperclass();
        while (superType != null && superType.getKind() != TypeKind.NONE) {
            Element superElement = processingEnv.getTypeUtils().asElement(superType);
            if (!(superElement instanceof TypeElement)) {
                return false;
            }
            TypeElement superTypeElement = (TypeElement) superElement;
            if (APP_HOOK.equals(superTypeElement.getQualifiedName().toString())) {
                return true;
            }
            superType = superTypeElement.getSuperclass();
        }
        return false;
    }

    private static boolean hasPublicNoArgConstructor(TypeElement type) {
        boolean sawConstructor = false;
        for (Element enclosed : type.getEnclosedElements()) {
            if (enclosed.getKind() != ElementKind.CONSTRUCTOR) {
                continue;
            }
            sawConstructor = true;
            ExecutableElement constructor = (ExecutableElement) enclosed;
            if (constructor.getModifiers().contains(Modifier.PUBLIC)
                    && constructor.getParameters().isEmpty()) {
                return true;
            }
        }
        return !sawConstructor;
    }

    private static JavaFile registry(Set<String> hooks) {
        ClassName appHook = ClassName.get(PACKAGE, "AppHook");
        TypeName mapType = ParameterizedTypeName.get(ClassName.get(Map.class), ClassName.get(String.class), appHook);
        MethodSpec.Builder register = MethodSpec.methodBuilder("register")
                .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                .addParameter(mapType, "hooks");
        for (String hook : hooks) {
            register.addStatement("register(hooks, new $T())", ClassName.bestGuess(hook));
        }
        MethodSpec registerOne = MethodSpec.methodBuilder("register")
                .addModifiers(Modifier.PRIVATE, Modifier.STATIC)
                .addParameter(mapType, "hooks")
                .addParameter(appHook, "hook")
                .addStatement("hooks.put(hook.packageName(), hook)")
                .build();
        TypeSpec type = TypeSpec.classBuilder(REGISTRY)
                .addModifiers(Modifier.PUBLIC, Modifier.FINAL)
                .addMethod(MethodSpec.constructorBuilder().addModifiers(Modifier.PRIVATE).build())
                .addMethod(register.build())
                .addMethod(registerOne)
                .build();
        return JavaFile.builder(PACKAGE, type).build();
    }
}
